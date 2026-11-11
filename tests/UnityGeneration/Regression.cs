using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using Debug = UnityEngine.Debug;

static class Program
{
    static int scenarios;
    static readonly string ModelDirectory = Path.Combine(Path.GetTempPath(), "HeightmapGenRegression");
    static void Set(TerrainGen terrain, string name, object value) =>
        typeof(TerrainGen).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).SetValue(terrain, value);
    static void Invoke(TerrainGen terrain, string name) =>
        typeof(TerrainGen).GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic).Invoke(terrain, null);
    static void Pump(TerrainGen terrain, Func<bool> done)
    {
        var clock = Stopwatch.StartNew();
        while (!done())
        {
            Check.That(clock.ElapsedMilliseconds < 10000, "timed out");
            terrain.Frame();
            Thread.Sleep(1);
        }
    }
    static TerrainGen New(TerrainGen.GeneratorType type = TerrainGen.GeneratorType.Perlin,
        bool cache = true, int workers = 3, int chunks = 3)
    {
        HeightmapGenAPI.Reset(); Chunk.Meshes.Clear(); Debug.Errors = 0;
        var terrain = new TerrainGen();
        Set(terrain, "started", true);
        Set(terrain, "chunkObject", new GameObject());
        Set(terrain, "resolution", 8); Set(terrain, "chunkCount", new Vector2Int(chunks, 1));
        Set(terrain, "generatorType", type); Set(terrain, "cacheable", cache); Set(terrain, "workerAmount", workers);
        terrain.Regenerate();
        return terrain;
    }
    static void Finish(TerrainGen terrain)
    {
        HeightmapGenAPI.FirstChunkGate.Set();
        HeightmapGenAPI.CleanupGate.Set();
        HeightmapGenAPI.Gate.Release(10000);
        Pump(terrain, () => !TerrainGen.IsGenerating);
        Check.That(HeightmapGenAPI.LiveHandles == 0, "native resources released on completion");
        Check.That(HeightmapGenAPI.MainThreadCalls == 0, "native work touched Unity thread");
        Invoke(terrain, "OnDestroy");
    }
    static void Snapshot(TerrainGen.GeneratorType type, bool cache, int workers)
    {
        var terrain = New(type, cache, workers);
        Pump(terrain, () => HeightmapGenAPI.Calls >= (cache && workers > 0 ? 3 : 1));
        Set(terrain, "resolution", 3); Set(terrain, "scale", 7f); Set(terrain, "amplitude", 12f);
        Set(terrain, "chunkCount", new Vector2Int(9, 9)); Set(terrain, "workerAmount", 0);
        Finish(terrain);
        Check.That(Chunk.Meshes.Count == 3 && Chunk.Meshes.All(m => m.Size == 8 && m.Scale == 2 && m.Amplitude == 5),
            "settings snapshot changed while generating");
        Check.That(terrain.CompletedChunkCount == 3 && terrain.AverageHeight == 8 && terrain.AverageSlope == 0,
            "statistics do not match displayed chunks");
        Check.That(Debug.Errors == 0, "unexpected generation failure");
        Check.That(HeightmapGenAPI.LastWorkers == (cache ? workers : 0), "fallback created unused workers");
        Check.That(cache && workers > 0 ? HeightmapGenAPI.Peak == 3 : HeightmapGenAPI.Peak == 1, "wrong concurrency");
        if (!cache) Check.That(HeightmapGenAPI.Requests == 0, "uncached mode tried request/probe");
        Check.That(Math.Abs(terrain.AverageChunkGenerationSeconds * terrain.CompletedChunkCount
            - terrain.ElapsedGenerationSeconds) < 1e-9,
            "average is not elapsed per completed chunk");
        double elapsed = terrain.ElapsedGenerationSeconds; Thread.Sleep(5);
        Check.That(elapsed == terrain.ElapsedGenerationSeconds, "finished timer still running");
        scenarios++;
    }
    static void Restart(bool cache)
    {
        var terrain = New(TerrainGen.GeneratorType.HydraulicErosion, cache);
        Pump(terrain, () => HeightmapGenAPI.Calls >= (cache ? 3 : 1));
        int previousCalls = HeightmapGenAPI.Calls;
        for (int i = 0; i < 10; i++)
        {
            Set(terrain, "resolution", i + 4); Set(terrain, "chunkCount", new Vector2Int(1, 1));
            Set(terrain, "workerAmount", 0);
            var clock = Stopwatch.StartNew();
            terrain.Regenerate(); terrain.Frame();
            Check.That(clock.ElapsedMilliseconds < 100, "restart blocked on old native jobs");
        }
        Check.That(HeightmapGenAPI.Calls == previousCalls && HeightmapGenAPI.ContextPeak == 1, "overlapping native sessions");
        Check.That(terrain.CompletedChunkCount == 0 && terrain.ElapsedGenerationSeconds == 0, "old work changed new statistics");
        Finish(terrain);
        Check.That(Chunk.Meshes.Count == 1 && Chunk.Meshes[0].Size == 13 && terrain.HighestHeight == 13,
            "obsolete result reached new terrain");
        Check.That(HeightmapGenAPI.ContextPeak == 1 && Debug.Errors == 0, "restart cleanup failed");
        scenarios++;
    }
    static void SlowCleanup()
    {
        var terrain = New(chunks: 1);
        HeightmapGenAPI.CleanupGate.Reset();
        HeightmapGenAPI.Gate.Release();
        Pump(terrain, () => HeightmapGenAPI.CleanupCalls > 0);
        var clock = Stopwatch.StartNew(); terrain.Regenerate(); terrain.Frame();
        Check.That(clock.ElapsedMilliseconds < 100, "restart blocked during native cleanup");
        Check.That(HeightmapGenAPI.ContextPeak == 1, "new context before old cleanup");
        Finish(terrain);
        Check.That(terrain.CompletedChunkCount == 1 && HeightmapGenAPI.ContextPeak == 1, "cleanup restart failed");
        scenarios++;
    }
    static void BoundedQueueAndCancellation()
    {
        var terrain = New(chunks: 1000);
        terrain.Frame(); HeightmapGenAPI.Gate.Release(10000);
        // Deliberately stop consuming meshes while the producer fills its queue.
        var clock = Stopwatch.StartNew();
        while (HeightmapGenAPI.Requests < 6 && clock.ElapsedMilliseconds < 5000) Thread.Sleep(1);
        Check.That(HeightmapGenAPI.Requests == 6, "expected both bounded windows to fill");
        Thread.Sleep(30);
        Check.That(HeightmapGenAPI.Requests == 6, "producer queued the whole terrain");
        Set(terrain, "chunkCount", new Vector2Int(1, 1));
        terrain.Regenerate();
        Finish(terrain);
        Check.That(Chunk.Meshes.Count == 1 && terrain.CompletedChunkCount == 1, "canceled buffered results leaked");
        scenarios++;
    }
    static void ReadyChunksDoNotWaitForFirst()
    {
        var terrain = New();
        HeightmapGenAPI.HoldFirstChunk = true;
        HeightmapGenAPI.FirstChunkGate.Reset(); HeightmapGenAPI.Gate.Release(3);
        Pump(terrain, () => terrain.CompletedChunkCount == 2);
        Check.That(!HeightmapGenAPI.FirstChunkGate.IsSet, "test released first chunk early");
        Finish(terrain);
        Check.That(terrain.CompletedChunkCount == 3, "lost delayed result");
        scenarios++;
    }
    static void Failure(bool creation)
    {
        var terrain = New(chunks: 1);
        HeightmapGenAPI.FailCreation = creation;
        HeightmapGenAPI.FailRead = !creation;
        Finish(terrain);
        Check.That(Debug.Errors == 1 && terrain.CompletedChunkCount == 0, "failure not observed or cleanup incomplete");
        scenarios++;
    }
    static void DisableEnableAndShutdown()
    {
        var terrain = New();
        Pump(terrain, () => HeightmapGenAPI.Calls == 3);
        var clock = Stopwatch.StartNew(); Invoke(terrain, "OnDisable");
        Check.That(clock.ElapsedMilliseconds < 100 && !TerrainGen.IsGenerating, "disable blocked");
        Set(terrain, "resolution", 5); Set(terrain, "chunkCount", new Vector2Int(1, 1));
        Invoke(terrain, "OnEnable");
        Finish(terrain);
        Check.That(Chunk.Meshes.Count == 1 && Chunk.Meshes[0].Size == 5, "enable did not restart safely");

        terrain = New();
        Pump(terrain, () => HeightmapGenAPI.Calls == 3);
        var release = Task.Run(() => { Thread.Sleep(30); HeightmapGenAPI.Gate.Release(100); });
        Invoke(terrain, "OnDestroy"); release.GetAwaiter().GetResult();
        Check.That(HeightmapGenAPI.LiveHandles == 0 && !TerrainGen.IsGenerating, "shutdown left native work alive");
        scenarios++;
    }
    static void KeyboardAppliesDraft()
    {
        var terrain = New();
        int commits = 0;
        terrain.BeforeRegenerateChunks += () => { commits++; Set(terrain, "resolution", 5); Set(terrain, "chunkCount", new Vector2Int(1, 1)); };
        UnityEngine.InputSystem.Keyboard.current = new UnityEngine.InputSystem.Keyboard();
        UnityEngine.InputSystem.Keyboard.current[UnityEngine.InputSystem.Key.R].wasPressedThisFrame = true;
        Invoke(terrain, "Update"); UnityEngine.InputSystem.Keyboard.current = null;
        Finish(terrain);
        Check.That(commits == 1 && Chunk.Meshes.Count == 1 && Chunk.Meshes[0].Size == 5, "R did not commit draft before snapshot");
        scenarios++;
    }
    static void ErosionPadding()
    {
        var terrain = New(TerrainGen.GeneratorType.HydraulicErosion, workers: 0, chunks: 1);
        Set(terrain, "padding", 6);
        terrain.Regenerate();
        Pump(terrain, () => HeightmapGenAPI.LastErosionPadding == 6);
        Finish(terrain);
        Check.That(Debug.Errors == 0, "erosion padding did not reach the native generator");
        scenarios++;
    }
    static void EmptyAndInvalid()
    {
        var terrain = New(chunks: 0); Finish(terrain);
        Check.That(terrain.CompletedChunkCount == 0 && HeightmapGenAPI.Calls == 0, "empty terrain scheduled work");
        terrain = New(); Set(terrain, "resolution", int.MaxValue); terrain.Regenerate(); Finish(terrain);
        Check.That(Debug.Errors == 1 && HeightmapGenAPI.LiveHandles == 0, "overflow reached native API");
        scenarios++;
    }
    static void StatisticsMath()
    {
        void Near(double actual, double expected, string label) => Check.That(Math.Abs(actual - expected) < 1e-9, label + ": " + actual);
        void Add(TerrainGen terrain, float[] heights, int size, float spacing) => typeof(TerrainGen)
            .GetMethod("CollectTerrainStatistics", BindingFlags.Instance | BindingFlags.NonPublic)
            .Invoke(terrain, new object[] { heights, size, spacing });

        var flat = new TerrainGen(); Add(flat, new float[] { -4, -4, -4, -4 }, 2, 2);
        Near(flat.AverageHeight, -4, "negative flat mean"); Near(flat.MaximumSlope, 0, "flat slope");
        Check.That(flat.HighestHeight == -4 && flat.LowestHeight == -4, "negative extrema");

        var plane = new TerrainGen(); Add(plane, new float[] { 0, 0, 0, 2, 2, 2, 4, 4, 4 }, 3, 2);
        Near(plane.AverageHeight, 2, "plane mean"); Near(plane.AverageSlope, 45, "plane slope"); Near(plane.MaximumSlope, 45, "plane max slope");

        double diagonalSlope = Math.Atan(Math.Sqrt(2)) * 180 / Math.PI;
        var diagonal = new TerrainGen(); Add(diagonal, new float[] { 0, 2, 2, 4 }, 2, 2);
        Near(diagonal.AverageHeight, 2, "diagonal mean"); Near(diagonal.AverageSlope, diagonalSlope, "diagonal gradient magnitude");

        var corner = new TerrainGen(); Add(corner, new float[] { 0, 0, 0, 2 }, 2, 2);
        Near(corner.AverageHeight, 1.0 / 3, "triangle area weighted height");
        Near(corner.AverageSlope, diagonalSlope / 2, "triangle mean slope"); Near(corner.MaximumSlope, diagonalSlope, "triangle max slope");

        var joined = new TerrainGen(); Add(joined, new float[] { 0, 0, 2, 2 }, 2, 2); Add(joined, new float[] { 2, 2, 4, 4 }, 2, 2);
        Near(joined.AverageHeight, 2, "joined chunks mean"); Near(joined.AverageSlope, 45, "joined chunks slope");

        var point = new TerrainGen(); Add(point, new float[] { 7 }, 1, 2);
        Near(point.AverageHeight, 7, "single sample mean"); Check.That(!point.HasSlopeStatistics, "single sample has no slope");
    }

    static void Main()
    {
        Directory.CreateDirectory(ModelDirectory);
        File.WriteAllBytes(Path.Combine(ModelDirectory, "best_step6126.onnx"), Array.Empty<byte>());
        File.WriteAllBytes(Path.Combine(ModelDirectory, "erosion_model.onnx"), Array.Empty<byte>());
        UnityEngine.Application.streamingAssetsPath = ModelDirectory;
        foreach (bool dispose in new[] { false, true })
        {
            MonoBehaviour.DisposeOnStop = dispose;
            foreach (var type in Enum.GetValues<TerrainGen.GeneratorType>())
                foreach (bool cache in new[] { false, true })
                    foreach (int workers in new[] { 0, 3 }) Snapshot(type, cache, workers);
            Restart(false); Restart(true); SlowCleanup(); BoundedQueueAndCancellation();
            ReadyChunksDoNotWaitForFirst(); Failure(false); Failure(true);
            DisableEnableAndShutdown(); KeyboardAppliesDraft(); ErosionPadding(); EmptyAndInvalid();
        }
        StatisticsMath();
        Console.WriteLine($"PASS: {scenarios} generation/lifecycle scenarios and 6 analytic statistics cases.");
    }
}
