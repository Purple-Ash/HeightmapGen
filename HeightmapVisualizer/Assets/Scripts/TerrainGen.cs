using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using Stopwatch = System.Diagnostics.Stopwatch;
using UnityEngine;
using UnityEngine.InputSystem;

public class TerrainGen : MonoBehaviour
{
    public enum GeneratorType
    {
        HydraulicErosion,
        Perlin,
        BrownianNoise,
        BPGenerator,
        ModelErosion
    }

    [Header("Terrain Common Settings")]
    [SerializeField] private GameObject chunkObject;
    [SerializeField] private Vector2Int chunkCount = new Vector2Int(1, 1);
    [Min(1)][SerializeField] private int resolution = 16;
    [Min(0.0001f)][SerializeField] private float scale = 2.0f;
    [SerializeField] private float amplitude = 5.0f;
    [SerializeField] private ulong seed = 0;
    [SerializeField] private bool cacheable = true;
    [SerializeField] private GeneratorType generatorType = GeneratorType.HydraulicErosion;

    [Header("Generation Scheduling")]
    [Range(0, 64)][SerializeField] private int workerAmount = 3;

    [Header("BP Generator Settings")]
    [Tooltip("ONNX model path, either absolute or relative to StreamingAssets. Resolution must match the model output.")]
    [SerializeField] private string bpModelPath = "best_step6126.onnx";

    [Header("Model Erosion Settings")]
    [Tooltip("ONNX model path, either absolute or relative to StreamingAssets. Resolution must match the model output.")]
    [SerializeField] private string erosionModelPath = "erosion_model.onnx";

    [Header("Blender")]
    [Tooltip("Blend chunk edges using the selected generator as the source.")]
    [SerializeField] private bool useBlender;
    [Header("Blender Settings")]
    [Min(1)][SerializeField] private int blendingRadius = 3;
    [Min(1)][SerializeField] private int edgeSamplingRadius = 2;

    [Header("Hydraulic Erosion Settings")]
    [SerializeField] private int erosionSeed;
    [Min(0)][SerializeField] private int erosionIterations = 10;
    [Min(1)][SerializeField] private int erosionRadius = 3;
    [Min(1)][SerializeField] private int maxDropletLifetime = 30;
    [Min(0)][SerializeField] private int padding = 30;
    [Range(0.0f, 1.0f)][SerializeField] private float inertia = 0.05f;
    [Min(0.0f)][SerializeField] private float sedimentCapacityFactor = 4.0f;
    [Min(0.0f)][SerializeField] private float minSedimentCapacity = 0.01f;
    [Range(0.0f, 1.0f)][SerializeField] private float erodeSpeed = 0.3f;
    [Range(0.0f, 1.0f)][SerializeField] private float depositSpeed = 0.3f;
    [Range(0.0f, 1.0f)][SerializeField] private float evaporateSpeed = 0.01f;
    [Min(0.0f)][SerializeField] private float gravity = 4.0f;
    [Min(0.0f)][SerializeField] private float initialSpeed = 1.0f;
    [Min(0.0f)][SerializeField] private float initialWaterVolume = 1.0f;

    public static bool IsGenerating { get; private set; }
    public bool KeyboardShortcutsEnabled { get; set; } = true;
    public event Action BeforeRegenerateChunks;
    public event Action<Chunk> ChunkGenerated;
    public bool HasHeightStatistics { get; private set; }
    public float HighestHeight { get; private set; }
    public double MaximumSlope { get; private set; }
    public double AverageHeight { get; private set; }
    public double AverageSlope { get; private set; }
    public float LowestHeight { get; private set; }
    public bool HasSlopeStatistics => triangleCount > 0;
    public double ElapsedGenerationSeconds => generationTimer.Elapsed.TotalSeconds;
    public int CompletedChunkCount { get; private set; }
    public double AverageChunkGenerationSeconds => CompletedChunkCount > 0 ? ElapsedGenerationSeconds / CompletedChunkCount : 0.0;

    private readonly Stopwatch generationTimer = new Stopwatch();
    private double sampleHeightSum;
    private long sampleCount;
    private double triangleHeightSum;
    private long triangleCount;
    private double slopeSum;

    private readonly List<GameObject> spawnedChunks = new List<GameObject>();
    private Coroutine generationCoroutine;
    private TerrainGenerationSession generationSession;
    private int numberOfGeneration;
    private bool started;

    private void Awake()
    {
#if UNITY_EDITOR
        UnityEditor.AssemblyReloadEvents.beforeAssemblyReload += Shutdown;
        UnityEditor.EditorApplication.playModeStateChanged += OnPlayModeStateChanged;
#endif
    }

    private void Start()
    {
        started = true;
        Regenerate();
    }

    private void OnEnable()
    {
        if (started) Regenerate();
    }

    private void Update()
    {
        if (generationCoroutine == null && generationSession != null && generationSession.Completion.IsCompleted)
        {
            ReleaseCompletedSession();
        }

        var keyboard = Keyboard.current;
        if (KeyboardShortcutsEnabled && keyboard != null && keyboard[Key.R].wasPressedThisFrame)
        {
            Regenerate();
        }
    }

    public void Regenerate()
    {
        BeforeRegenerateChunks?.Invoke();
        StopGeneration();
        ClearChunks();

        HasHeightStatistics = false;
        HighestHeight = 0f;
        AverageHeight = 0.0;
        LowestHeight = 0f;
        MaximumSlope = 0.0;
        AverageSlope = 0.0;
        sampleHeightSum = 0.0;
        sampleCount = 0;
        slopeSum = 0.0;
        triangleHeightSum = 0.0;
        triangleCount = 0;
        CompletedChunkCount = 0;

        generationTimer.Reset();
        IsGenerating = true;
        generationCoroutine = StartCoroutine(Generate(numberOfGeneration));
    }

    private void StopGeneration()
    {

        numberOfGeneration++;
        generationSession?.Cancel();
        if (generationCoroutine != null)
        {
            StopCoroutine(generationCoroutine);
            generationCoroutine = null;
        }
        generationTimer.Stop();
        IsGenerating = false;
    }

    private void ClearChunks()
    {
        foreach (var chunkGO in spawnedChunks)
        {
            if (chunkGO != null)
            {
                chunkGO.SetActive(false);
                Destroy(chunkGO);
            }
        }
        spawnedChunks.Clear();
    }

    private void ReleaseCompletedSession()
    {
        var finished = generationSession;
        generationSession = null;
        if (finished == null) return;
        try { 
            finished.Completion.GetAwaiter().GetResult(); 
        }
        catch (Exception exception) { 
            Debug.LogException(exception); 
        }
        finally { 
            finished.Dispose(); 
        }
    }

    private IEnumerator Generate(int version)
    {
        Vector2Int generationChunkCount = chunkCount;
        float generationScale = scale;
        GameObject generationChunkObject = chunkObject;
        GeneratorType generationType = generatorType;
        bool generationUsesBlender = useBlender;
        HeightmapGenAPI.CommonSettings commonSettings = new HeightmapGenAPI.CommonSettings
        {
            seed = seed,
            scale = scale /  100f,
            amplitude = amplitude,
            resolution = resolution,
            cacheable = cacheable
        };

        HeightmapGenAPI.HydraulicErosionSettings erosionSettings = new HeightmapGenAPI.HydraulicErosionSettings {
            seed = erosionSeed,
            numIterations = erosionIterations,
            erosionRadius = erosionRadius,
            maxDropletLifetime = maxDropletLifetime,
            padding = padding,
            inertia = inertia,
            sedimentCapacityFactor = sedimentCapacityFactor,
            minSedimentCapacity = minSedimentCapacity,
            erodeSpeed = erodeSpeed,
            depositSpeed = depositSpeed,
            evaporateSpeed = evaporateSpeed,
            gravity = gravity,
            initialSpeed = initialSpeed,
            initialWaterVolume = initialWaterVolume
        };

        string resolvedBPModelPath = ResolveModelPath(bpModelPath);
        string resolvedErosionModelPath = ResolveModelPath(erosionModelPath);
        HeightmapGenAPI.BPGeneratorSettings bpSettings = new HeightmapGenAPI.BPGeneratorSettings
        {
            modelPath = resolvedBPModelPath
        };
        HeightmapGenAPI.ModelErosionSettings modelErosionSettings = new HeightmapGenAPI.ModelErosionSettings
        {
            modelPath = resolvedErosionModelPath
        };
        HeightmapGenAPI.BlenderSettings blenderSettings = new HeightmapGenAPI.BlenderSettings
        {
            blendingRadius = blendingRadius,
            edgeSamplingRadius = edgeSamplingRadius
        };

        int generationWorkerCount = workerAmount;

        try
        {
            yield return null;

            while (generationSession != null && !generationSession.Completion.IsCompleted)
                yield return null;
            ReleaseCompletedSession();

            if (generationChunkObject == null)
            {
                Debug.LogError("No chunk object assigned!");
                yield break;
            }

            if (generationType == GeneratorType.BPGenerator &&
                (resolvedBPModelPath == null || !File.Exists(resolvedBPModelPath)))
            {
                Debug.LogError($"BP Generator ONNX model not found: {resolvedBPModelPath ?? "(empty path)"}");
                yield break;
            }

            if (generationType == GeneratorType.ModelErosion &&
                (resolvedErosionModelPath == null || !File.Exists(resolvedErosionModelPath)))
            {
                Debug.LogError($"Model erosion ONNX model not found: {resolvedErosionModelPath ?? "(empty path)"}");
                yield break;
            }

            try
            {
                generationSession = new TerrainGenerationSession(generationType, commonSettings, erosionSettings,
                    bpSettings, modelErosionSettings, generationUsesBlender, blenderSettings,
                    generationChunkCount.x, generationChunkCount.y,
                    generationWorkerCount);
            }
            catch (Exception exception)
            {
                Debug.LogException(exception);
            }
            if (generationSession == null) yield break;

            generationTimer.Start();
            var run = generationSession;
            while (true)
            {
                if (run.TryTake(out var result))
                {
                    ApplyResult(result, generationChunkObject, commonSettings, generationScale);
                    yield return null;
                }
                else if (run.Completion.IsCompleted)
                {
                    if (run.TryTake(out var lastResult))
                    {
                        ApplyResult(lastResult, generationChunkObject, commonSettings, generationScale);
                        yield return null;
                        continue;
                    }
                    ReleaseCompletedSession();
                    break;
                }
                else
                {
                    yield return null;
                }
            }
        }
        finally
        {
            if (version == numberOfGeneration)
            {
                generationSession?.Cancel();
                generationTimer.Stop();

                IsGenerating = false;
                generationCoroutine = null;

            }
        }
    }

    private static string ResolveModelPath(string modelPath)
    {
        if (string.IsNullOrWhiteSpace(modelPath)) return null;
        return Path.IsPathRooted(modelPath)
            ? modelPath
            : Path.Combine(Application.streamingAssetsPath, modelPath);
    }

    private void ApplyResult(TerrainGenerationSession.Result result, GameObject prefab,
        HeightmapGenAPI.CommonSettings settings, float spacing)
    {
        Vector3 chunkOffset = new Vector3(result.X * (settings.resolution - 1) * spacing, 0,
            result.Y * (settings.resolution - 1) * spacing);
        GameObject instance = Instantiate(prefab, chunkOffset, Quaternion.identity, transform);
        spawnedChunks.Add(instance);
        if (instance.TryGetComponent<Chunk>(out var chunk))
        {
            chunk.generateMesh(result.Heights, settings.resolution, settings.resolution, spacing, settings.amplitude);
            ChunkGenerated?.Invoke(chunk);
        }
        CollectTerrainStatistics(result.Heights, settings.resolution, spacing);
        CompletedChunkCount++;
    }

    private void CollectTerrainStatistics(float[] heights, int size, float spacing)
    {
        foreach (float height in heights)
        {
            if (!HasHeightStatistics || height > HighestHeight) HighestHeight = height;
            if (!HasHeightStatistics || height < LowestHeight) LowestHeight = height;
            sampleHeightSum += height;
            sampleCount++;
            HasHeightStatistics = true;
        }

        if (spacing != 0f)
        {
            for (int x = 0; x < size - 1; x++)
            {
                for (int y = 0; y < size - 1; y++)
                {
                    int index = x * size + y;
                    double h00 = heights[index];
                    double h01 = heights[index + 1];
                    double h10 = heights[index + size];
                    double h11 = heights[index + size + 1];
                    AddTrianglesStatistics(h00, h01, h10, (h10 - h00) / spacing, (h01 - h00) / spacing);
                    AddTrianglesStatistics(h10, h01, h11, (h11 - h01) / spacing, (h11 - h10) / spacing);
                }
            }
        }

        AverageHeight = triangleCount > 0 ? triangleHeightSum / triangleCount
            : sampleCount > 0 ? sampleHeightSum / sampleCount : 0.0;
        AverageSlope = triangleCount > 0 ? slopeSum / triangleCount : 0.0;
    }

    private void AddTrianglesStatistics(double a, double b, double c, double gradientX, double gradientY)
    {
        double slope = Math.Atan(Math.Sqrt(gradientX * gradientX + gradientY * gradientY)) * (180.0 / Math.PI);
        triangleHeightSum += (a + b + c) / 3.0;
        slopeSum += slope;
        triangleCount++;
        if (slope > MaximumSlope) MaximumSlope = slope;
    }

    private void OnDisable()
    {
        StopGeneration();
        ClearChunks();
    }

    private void Shutdown()
    {
        StopGeneration();
        ReleaseCompletedSession();
        ClearChunks();
    }

#if UNITY_EDITOR
    private void OnPlayModeStateChanged(UnityEditor.PlayModeStateChange state)
    {
        if (state == UnityEditor.PlayModeStateChange.ExitingPlayMode) Shutdown();
    }
#endif

    private void OnDestroy()
    {
#if UNITY_EDITOR
        UnityEditor.AssemblyReloadEvents.beforeAssemblyReload -= Shutdown;
        UnityEditor.EditorApplication.playModeStateChanged -= OnPlayModeStateChanged;
#endif
        Shutdown();
    }
}
