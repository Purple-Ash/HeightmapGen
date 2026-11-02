using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

public class TerrainGen
{
    public enum GeneratorType { HydraulicErosion, Perlin, BrownianNoise }
}

static class NativeSmoke
{
    static HeightmapGenAPI.CommonSettings Settings(bool cache) =>
        new HeightmapGenAPI.CommonSettings { seed = 42, scale = .03f, amplitude = 5, resolution = 24, cacheable = cache };
    static HeightmapGenAPI.HydraulicErosionSettings Erosion()
    {
        var settings = HeightmapGenAPI.HydraulicErosionSettings.Default;
        settings.numIterations = 2;
        return settings;
    }
    static Dictionary<(int, int), float[]> Reference(TerrainGen.GeneratorType type, bool cache)
    {
        var settings = Settings(cache);
        var ctx = HeightmapGenAPI.createContext(0);
        IntPtr baseGen = IntPtr.Zero, gen = IntPtr.Zero;
        try
        {
            if (type == TerrainGen.GeneratorType.Perlin) gen = HeightmapGenAPI.createPerlinGenerator(ctx, settings);
            else if (type == TerrainGen.GeneratorType.BrownianNoise) gen = HeightmapGenAPI.createBrownianPerlinGenerator(ctx, settings);
            else
            {
                var baseSettings = settings; baseSettings.amplitude = 1;
                baseGen = HeightmapGenAPI.createBrownianPerlinGenerator(ctx, baseSettings);
                var erosion = Erosion(); erosion.baseGeneratorImpl = baseGen;
                gen = HeightmapGenAPI.createHydraulicErosionGenerator(ctx, settings, erosion);
            }
            var output = new Dictionary<(int, int), float[]>();
            for (int x = -1; x <= 1; x++) for (int y = -1; y < 1; y++)
            {
                var data = new float[24 * 24];
                HeightmapGenAPI.getChunk(gen, x, y, data);
                output.Add((x, y), data);
            }
            return output;
        }
        finally
        {
            if (gen != IntPtr.Zero) HeightmapGenAPI.destroyGenerator(gen);
            if (baseGen != IntPtr.Zero) HeightmapGenAPI.destroyGenerator(baseGen);
            HeightmapGenAPI.destroyContext(ctx);
        }
    }
    static void Main(string[] args)
    {
        if (args.Length != 1) throw new ArgumentException("Pass the absolute path to the rebuilt native library.");
        NativeLibrary.SetDllImportResolver(typeof(HeightmapGenAPI).Assembly,
            (name, assembly, searchPath) => name == "HeightmapGen" ? NativeLibrary.Load(args[0]) : IntPtr.Zero);
        int cases = 0;
        foreach (var type in Enum.GetValues<TerrainGen.GeneratorType>())
        foreach (bool cache in new[] { false, true })
        {
            var expected = Reference(type, cache);
            foreach (int workers in new[] { 0, 1, 3 })
            {
                using var session = new TerrainGenerationSession(type, Settings(cache), Erosion(), 3, 2, workers);
                var output = new Dictionary<(int, int), float[]>();
                var timeout = Stopwatch.StartNew();
                while (true)
                {
                    if (session.TryTake(out var result)) output.Add((result.X, result.Y), result.Heights);
                    else if (session.Completion.IsCompleted)
                    {
                        while (session.TryTake(out result)) output.Add((result.X, result.Y), result.Heights);
                        break;
                    }
                    else Thread.Sleep(1);
                    if (timeout.Elapsed.TotalSeconds > 20) throw new TimeoutException();
                }
                session.Completion.GetAwaiter().GetResult();
                if (output.Count != expected.Count || expected.Any(pair => !output[pair.Key].SequenceEqual(pair.Value)))
                    throw new Exception($"Output differs for {type}, cache={cache}, workers={workers}");
                cases++;
            }
        }
        // Cancel with outstanding requests / a full consumer queue. Disposal is only
        // legal after all native teardown has completed.
        for (int attempt = 0; attempt < 20; attempt++)
        {
            using var session = new TerrainGenerationSession(TerrainGen.GeneratorType.HydraulicErosion,
                Settings(true), Erosion(), 100, 100, 3);
            Thread.Sleep(attempt % 4);
            session.Cancel();
            if (!session.Completion.Wait(TimeSpan.FromSeconds(20))) throw new TimeoutException("Cancellation did not drain.");
            session.Completion.GetAwaiter().GetResult();
        }
        Console.WriteLine($"PASS: {cases} native/managed output comparisons and 20 cancellation/drain cycles.");
    }
}
