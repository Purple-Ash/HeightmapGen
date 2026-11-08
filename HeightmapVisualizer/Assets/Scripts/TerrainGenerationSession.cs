using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

internal sealed class TerrainGenerationSession : IDisposable
{
    internal readonly struct Result
    {
        public readonly int X;
        public readonly int Y;

        public readonly float[] Heights;

        public Result(int x, int y, float[] heights)
        {
            Heights = heights;
            X = x;
            Y = y;
        }
    }

    private const long BufferBudgetBytes = 64L * 1024 * 1024;
    private readonly CancellationTokenSource cancellation = new CancellationTokenSource();
    private readonly BlockingCollection<Result> results;
    public Task Completion { get; }

    public TerrainGenerationSession(TerrainGen.GeneratorType type, HeightmapGenAPI.CommonSettings settings,
        HeightmapGenAPI.HydraulicErosionSettings erosion, HeightmapGenAPI.BPGeneratorSettings bp,
        HeightmapGenAPI.ModelErosionSettings modelErosion, int width, int height, int workerCount)
    {
        if (settings.resolution < 1 || width < 0 || height < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(settings), "Invalid terrain dimensions");
        }

        if (workerCount < 0 || workerCount > 64)
        {
            throw new ArgumentOutOfRangeException(nameof(settings), "Invalid worker count");
        }

        int samples = checked(settings.resolution * settings.resolution);
        int count = checked(width * height);

        if (type == TerrainGen.GeneratorType.HydraulicErosion)
        {
            if (erosion.erosionRadius < 1 || erosion.numIterations < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(erosion));
            }

            int paddedSize = checked(settings.resolution + 2 * erosion.erosionRadius);
            _ = checked(paddedSize * paddedSize);
            _ = checked(samples * erosion.numIterations);
        }

        // Account for both queued managed results and cached native outputs
        // A single large chunk may exceed this budget
        // meshes/erosion scratch space are separate
        int bufferSlots = (int)Math.Max(1L, BufferBudgetBytes / (2L * sizeof(float) * samples));
        int window = Math.Max(1, Math.Min(Math.Min(count, Math.Max(1, workerCount)), bufferSlots));
        int nativeWorkers = settings.cacheable ? Math.Min(workerCount, window) : 0;
        results = new BlockingCollection<Result>(window);
        Completion = Task.Run(() => Generate(type, settings, erosion, bp, modelErosion,
            width, height, count, samples, window, nativeWorkers));
    }

    public bool TryTake(out Result result) => results.TryTake(out result);
    public void Cancel() => cancellation.Cancel();

    private void Generate(TerrainGen.GeneratorType type, HeightmapGenAPI.CommonSettings settings,
        HeightmapGenAPI.HydraulicErosionSettings erosion, HeightmapGenAPI.BPGeneratorSettings bp,
        HeightmapGenAPI.ModelErosionSettings modelErosion, int width, int height,
        int count, int samples, int window, int workers)
    {
        IntPtr context = IntPtr.Zero;
        IntPtr generator = IntPtr.Zero;
        IntPtr baseGenerator = IntPtr.Zero;
        CancellationToken token = cancellation.Token;
        try
        {
            token.ThrowIfCancellationRequested();
            if (count == 0) return;
            context = HeightmapGenAPI.createContext(workers);
            if (context == IntPtr.Zero) throw new InvalidOperationException("Failed to create HeightmapGen context.");

            switch (type)
            {
                case TerrainGen.GeneratorType.Perlin:
                    generator = HeightmapGenAPI.createPerlinGenerator(context, settings);
                    break;
                case TerrainGen.GeneratorType.BrownianNoise:
                    generator = HeightmapGenAPI.createBrownianPerlinGenerator(context, settings);
                    break;
                case TerrainGen.GeneratorType.BPGenerator:
                    generator = HeightmapGenAPI.createBPGenerator(context, settings, bp);
                    break;
                case TerrainGen.GeneratorType.ModelErosion:
                    var modelBaseSettings = settings;
                    modelBaseSettings.amplitude = 1f;
                    baseGenerator = HeightmapGenAPI.createBrownianPerlinGenerator(context, modelBaseSettings);
                    if (baseGenerator == IntPtr.Zero) throw new InvalidOperationException("Failed to create model erosion base generator.");
                    modelErosion.baseGeneratorImpl = baseGenerator;
                    generator = HeightmapGenAPI.createModelErosionGenerator(context, settings, modelErosion);
                    break;
                case TerrainGen.GeneratorType.HydraulicErosion:
                    var baseSettings = settings;
                    baseSettings.amplitude = 1f;
                    baseSettings.cacheable = false;
                    baseGenerator = HeightmapGenAPI.createBrownianPerlinGenerator(context, baseSettings);
                    if (baseGenerator == IntPtr.Zero) throw new InvalidOperationException("Failed to create erosion base generator.");
                    erosion.baseGeneratorImpl = baseGenerator;
                    generator = HeightmapGenAPI.createHydraulicErosionGenerator(context, settings, erosion);
                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(type));
            }
            if (generator == IntPtr.Zero) throw new InvalidOperationException("Failed to create terrain generator.");

            if (!settings.cacheable || workers == 0)
            {
                for (int index = 0; index < count; index++)
                {
                    token.ThrowIfCancellationRequested();
                    int x = index / height - width / 2;
                    int y = index % height - height / 2;
                    float[] data = new float[samples];
                    HeightmapGenAPI.getChunk(generator, x, y, data);
                    if (settings.cacheable) HeightmapGenAPI.cleanChunkFromCache(generator, x, y);
                    results.Add(new Result(x, y, data), token);
                }
                return;
            }

            var pending = new List<int>(window);
            int next = 0;
            while (next < count || pending.Count > 0)
            {
                token.ThrowIfCancellationRequested();
                while (next < count && pending.Count < window)
                {
                    token.ThrowIfCancellationRequested();
                    int index = next++;
                    HeightmapGenAPI.requestChunk(generator, index / height - width / 2, index % height - height / 2);
                    pending.Add(index);
                }

                bool received = false;
                for (int i = 0; i < pending.Count; i++)
                {
                    token.ThrowIfCancellationRequested();
                    int index = pending[i];
                    int x = index / height - width / 2;
                    int y = index % height - height / 2;
                    if (!HeightmapGenAPI.probeChunk(generator, x, y, null)) continue;
                    float[] data = new float[samples];

                    HeightmapGenAPI.getChunk(generator, x, y, data);
                    HeightmapGenAPI.cleanChunkFromCache(generator, x, y);
                    pending.RemoveAt(i);
                    results.Add(new Result(x, y, data), token);
                    received = true;
                    break;
                }
                if (!received) token.WaitHandle.WaitOne(1);
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
            // Restart discards this run, draining still happens below
        }
        finally
        {
            try
            {
                if (generator != IntPtr.Zero) HeightmapGenAPI.destroyGenerator(generator);
            }
            finally
            {
                try
                {
                    if (baseGenerator != IntPtr.Zero) HeightmapGenAPI.destroyGenerator(baseGenerator);
                }
                finally
                {
                    try
                    {
                        if (context != IntPtr.Zero) HeightmapGenAPI.destroyContext(context);
                    }
                    finally
                    {
                        if (token.IsCancellationRequested)
                        {
                            while (results.TryTake(out _)) { }
                        }

                        results.CompleteAdding();
                    }
                }
            }
        }
    }

    public void Dispose()
    {
        if (!Completion.IsCompleted) throw new InvalidOperationException("Wait for native cleanup before disposing a terrain session.");
        results.Dispose();
        cancellation.Dispose();
    }
}
