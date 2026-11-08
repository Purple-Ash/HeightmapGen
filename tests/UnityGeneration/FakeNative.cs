using System;
using System.Collections.Concurrent;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

public static class HeightmapGenAPI
{
    public struct CommonSettings
    {
        public ulong seed;
        public float scale, amplitude;
        public int resolution;
        public bool cacheable;
    }
    public struct HydraulicErosionSettings
    {
        public int seed, numIterations, erosionRadius, maxDropletLifetime;
        public float inertia, sedimentCapacityFactor, minSedimentCapacity, erodeSpeed, depositSpeed;
        public float evaporateSpeed, gravity, initialSpeed, initialWaterVolume;
        public IntPtr baseGeneratorImpl;
    }
    public struct BPGeneratorSettings { public string modelPath; }
    public struct ModelErosionSettings { public IntPtr baseGeneratorImpl; public string modelPath; }
    sealed class Context
    {
        public int Workers;
        public SemaphoreSlim Pool;
    }
    sealed class Generator
    {
        public IntPtr Context, Base;
        public CommonSettings Settings;
        public ConcurrentDictionary<(int, int), Task<float[]>> Jobs = new();
    }
    static readonly ConcurrentDictionary<IntPtr, Context> contexts = new();
    static readonly ConcurrentDictionary<IntPtr, Generator> generators = new();
    static int next, calls, requests, active, peak, mainThread, contextPeak, cleanupCalls;
    public static SemaphoreSlim Gate = new(0);
    public static ManualResetEventSlim CleanupGate = new(true);
    public static ManualResetEventSlim FirstChunkGate = new(true);
    public static bool FailCreation, FailRead, HoldFirstChunk;
    public static int LastWorkers, MainThreadCalls;
    public static int Calls => Volatile.Read(ref calls);
    public static int Requests => Volatile.Read(ref requests);
    public static int Peak => Volatile.Read(ref peak);
    public static int CleanupCalls => Volatile.Read(ref cleanupCalls);
    public static int ContextPeak => Volatile.Read(ref contextPeak);
    public static int LiveHandles => contexts.Count + generators.Count;
    static void Entry() { if (Environment.CurrentManagedThreadId == mainThread) Interlocked.Increment(ref MainThreadCalls); }
    public static void Reset()
    {
        Check.That(LiveHandles == 0 && active == 0, "previous run leaked");
        calls = requests = active = peak = contextPeak = cleanupCalls = MainThreadCalls = 0;
        mainThread = Environment.CurrentManagedThreadId;
        FailCreation = FailRead = HoldFirstChunk = false;
        Gate = new(0); CleanupGate = new(true); FirstChunkGate = new(true);
    }
    public static IntPtr createContext(int workers)
    {
        Entry(); LastWorkers = workers;
        var id = new IntPtr(Interlocked.Increment(ref next));
        contexts[id] = new Context { Workers = workers, Pool = new SemaphoreSlim(Math.Max(1, workers)) };
        contextPeak = Math.Max(contextPeak, contexts.Count);
        return id;
    }
    public static IntPtr createPerlinGenerator(IntPtr context, CommonSettings settings)
    {
        Entry();
        if (FailCreation) return IntPtr.Zero;
        var id = new IntPtr(Interlocked.Increment(ref next));
        generators[id] = new Generator { Context = context, Settings = settings };
        return id;
    }
    public static IntPtr createBrownianPerlinGenerator(IntPtr context, CommonSettings settings) => createPerlinGenerator(context, settings);
    public static IntPtr createBPGenerator(IntPtr context, CommonSettings settings, BPGeneratorSettings bp) =>
        createPerlinGenerator(context, settings);
    public static IntPtr createModelErosionGenerator(IntPtr context, CommonSettings settings, ModelErosionSettings modelErosion)
    {
        Check.That(generators.ContainsKey(modelErosion.baseGeneratorImpl), "model erosion base missing");
        var id = createPerlinGenerator(context, settings);
        if (id != IntPtr.Zero) generators[id].Base = modelErosion.baseGeneratorImpl;
        return id;
    }
    public static IntPtr createHydraulicErosionGenerator(IntPtr context, CommonSettings settings, HydraulicErosionSettings erosion)
    {
        Check.That(generators.ContainsKey(erosion.baseGeneratorImpl), "erosion base missing");
        var id = createPerlinGenerator(context, settings);
        if (id != IntPtr.Zero) generators[id].Base = erosion.baseGeneratorImpl;
        return id;
    }
    static float[] Compute(Generator generator, int x, int y)
    {
        var ctx = contexts[generator.Context];
        ctx.Pool.Wait();
        try
        {
            int now = Interlocked.Increment(ref active), old;
            do { old = peak; if (old >= now) break; } while (Interlocked.CompareExchange(ref peak, now, old) != old);
            Interlocked.Increment(ref calls);
            Gate.Wait();
            if (HoldFirstChunk && x == -1) FirstChunkGate.Wait();
            var data = new float[generator.Settings.resolution * generator.Settings.resolution];
            Array.Fill(data, (float)generator.Settings.resolution);
            return data;
        }
        finally { Interlocked.Decrement(ref active); ctx.Pool.Release(); }
    }
    public static void requestChunk(IntPtr id, int x, int y)
    {
        Entry();
        var gen = generators[id];
        if (!gen.Settings.cacheable) return;
        Interlocked.Increment(ref requests);
        gen.Jobs.GetOrAdd((x, y), key => contexts[gen.Context].Workers == 0
            ? Task.FromResult(Compute(gen, x, y)) : Task.Run(() => Compute(gen, x, y)));
    }
    public static bool probeChunk(IntPtr id, int x, int y, float[] buffer)
    {
        Entry();
        var gen = generators[id];
        if (!gen.Settings.cacheable || !gen.Jobs.TryGetValue((x, y), out var job) || !job.IsCompleted) return false;
        if (buffer != null) job.GetAwaiter().GetResult().CopyTo(buffer, 0);
        return true;
    }
    public static void getChunk(IntPtr id, int x, int y, float[] buffer)
    {
        Entry();
        var gen = generators[id];
        Check.That(buffer.Length == gen.Settings.resolution * gen.Settings.resolution, "buffer dimensions");
        if (gen.Settings.cacheable)
        {
            if (!gen.Jobs.ContainsKey((x, y))) requestChunk(id, x, y);
            gen.Jobs[(x, y)].GetAwaiter().GetResult().CopyTo(buffer, 0);
        }
        else Compute(gen, x, y).CopyTo(buffer, 0);
        if (FailRead) { FailRead = false; throw new InvalidOperationException("simulated read failure"); }
    }
    public static void cleanChunkFromCache(IntPtr id, int x, int y)
    {
        Entry(); generators[id].Jobs.TryRemove((x, y), out _);
    }
    public static void destroyGenerator(IntPtr id)
    {
        Entry(); Interlocked.Increment(ref cleanupCalls); CleanupGate.Wait();
        Check.That(!generators.Values.Any(g => g.Base == id), "base destroyed before dependent");
        var gen = generators[id];
        Task.WhenAll(gen.Jobs.Values).GetAwaiter().GetResult();
        Check.That(generators.TryRemove(id, out _), "double generator free");
    }
    public static void destroyContext(IntPtr id)
    {
        Entry();
        Check.That(!generators.Values.Any(g => g.Context == id), "context freed before generators");
        Check.That(contexts.TryRemove(id, out var ctx), "double context free");
        ctx.Pool.Dispose();
    }
}
