using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.InputSystem;
using Stopwatch = System.Diagnostics.Stopwatch;

public class TerrainGen : MonoBehaviour
{
    public enum GeneratorType
    {
        HydraulicErosion,
        Perlin,
        BrownianNoise
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

    [Header("Hydraulic Erosion Settings")]
    [SerializeField] private int erosionSeed;
    [Min(0)][SerializeField] private int erosionIterations = 10;
    [Min(1)][SerializeField] private int erosionRadius = 3;
    [Min(1)][SerializeField] private int maxDropletLifetime = 30;
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
    public event Action BeforeRegenerate;
    public bool HasHeightStatistics { get; private set; }
    public float HighestHeight { get; private set; }
    public float LowestHeight { get; private set; }
    public double AverageHeight { get; private set; }
    public bool HasSlopeStatistics => triangleCount > 0;
    public double AverageSlope { get; private set; }
    public double MaximumSlope { get; private set; }
    public double ElapsedGenerationSeconds => generationTimer.Elapsed.TotalSeconds;
    public int CompletedChunkCount { get; private set; }
    public double AverageChunkGenerationSeconds => CompletedChunkCount > 0
        ? totalChunkGenerationSeconds / CompletedChunkCount : 0.0;

    private readonly Stopwatch generationTimer = new Stopwatch();
    private double totalChunkGenerationSeconds;
    private double sampleHeightSum;
    private long sampleCount;
    private double triangleHeightSum;
    private double slopeSum;
    private long triangleCount;

    private IntPtr context = IntPtr.Zero;
    private IntPtr generator = IntPtr.Zero;
    private IntPtr baseGenerator = IntPtr.Zero;

    private readonly List<GameObject> spawnedChunks = new List<GameObject>();
    private Coroutine generationCoroutine;
    private Task activeChunkTask;

    void Start()
    {
        Regenerate();
    }

    void Update()
    {
        var keyboard = Keyboard.current;
        if (keyboard != null && keyboard[Key.R].wasPressedThisFrame)
        {
            Regenerate();
        }
    }

    public void Regenerate()
    {
        BeforeRegenerate?.Invoke();

        if (generationCoroutine != null)
        {
            StopCoroutine(generationCoroutine);
            generationCoroutine = null;
        }

        ClearChunks();
        HasHeightStatistics = false;
        HighestHeight = 0f;
        LowestHeight = 0f;
        AverageHeight = 0.0;
        AverageSlope = 0.0;
        MaximumSlope = 0.0;
        sampleHeightSum = 0.0;
        sampleCount = 0;
        triangleHeightSum = 0.0;
        slopeSum = 0.0;
        triangleCount = 0;
        CompletedChunkCount = 0;
        totalChunkGenerationSeconds = 0.0;
        generationTimer.Reset();
        IsGenerating = true;

        generationCoroutine = StartCoroutine(Generate());
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

    private void DestroyGenerators()
    {
        if (generator != IntPtr.Zero)
        {
            HeightmapGenAPI.destroyGenerator(generator);
            generator = IntPtr.Zero;
        }

        if (baseGenerator != IntPtr.Zero)
        {
            HeightmapGenAPI.destroyGenerator(baseGenerator);
            baseGenerator = IntPtr.Zero;
        }

        if (context != IntPtr.Zero)
        {
            HeightmapGenAPI.destroyContext(context);
            context = IntPtr.Zero;
        }
    }

    private IEnumerator Generate()
    {
        Vector2Int generationChunkCount = chunkCount;
        float generationScale = scale;
        GameObject generationChunkObject = chunkObject;
        GeneratorType generationType = generatorType;
        HeightmapGenAPI.CommonSettings commonSettings = new HeightmapGenAPI.CommonSettings
        {
            seed = seed,
            scale = scale / 100f,
            amplitude = amplitude,
            resolution = resolution,
            cacheable = cacheable
        };

        HeightmapGenAPI.HydraulicErosionSettings erosionSettings = new HeightmapGenAPI.HydraulicErosionSettings
        {
            seed = erosionSeed,
            numIterations = erosionIterations,
            erosionRadius = erosionRadius,
            maxDropletLifetime = maxDropletLifetime,
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

        try
        {
            yield return null;

            while (activeChunkTask != null && !activeChunkTask.IsCompleted)
            {
                yield return null;
            }
            if (activeChunkTask != null && activeChunkTask.IsFaulted)
            {
                Debug.LogException(activeChunkTask.Exception);
            }
            activeChunkTask = null;
            DestroyGenerators();

            generationTimer.Start();
            context = HeightmapGenAPI.createContext();
            if (context == IntPtr.Zero)
            {
                Debug.LogError("Failed to create HeightmapGen context.");
                yield break;
            }

            switch (generationType)
            {
                case GeneratorType.Perlin:
                    generator = HeightmapGenAPI.createPerlinGenerator(context, commonSettings);
                    break;

                case GeneratorType.BrownianNoise:
                    generator = HeightmapGenAPI.createBrownianPerlinGenerator(context, commonSettings);
                    break;

                case GeneratorType.HydraulicErosion:
                    HeightmapGenAPI.CommonSettings baseSettings = commonSettings;
                    baseSettings.amplitude = 1.0f;
                    baseGenerator = HeightmapGenAPI.createBrownianPerlinGenerator(context, baseSettings);
                    if (baseGenerator == IntPtr.Zero)
                    {
                        Debug.LogError("Failed to create base generator for hydraulic erosion");
                        yield break;
                    }

                    erosionSettings.baseGeneratorImpl = baseGenerator;
                    generator = HeightmapGenAPI.createHydraulicErosionGenerator(context, commonSettings, erosionSettings);
                    break;
            }

            if (generator == IntPtr.Zero)
            {
                Debug.LogError("Failed to create Generator instance");
                yield break;
            }

            for (int i = 0; i < generationChunkCount.x; i++)
            {
                for (int j = 0; j < generationChunkCount.y; j++)
                {
                    Stopwatch chunkTimer = Stopwatch.StartNew();
                    int chunkX = i - generationChunkCount.x / 2;
                    int chunkY = j - generationChunkCount.y / 2;
                    int totalSamples = checked(commonSettings.resolution * commonSettings.resolution);
                    float[] heightData = new float[totalSamples];

                    IntPtr chunkGenerator = generator;
                    activeChunkTask = Task.Run(() =>
                    {
                        HeightmapGenAPI.getChunk(chunkGenerator, chunkX, chunkY, heightData);
                    });

                    while (!activeChunkTask.IsCompleted)
                    {
                        yield return null;
                    }

                    bool faulted = activeChunkTask.IsFaulted;
                    Exception taskException = activeChunkTask.Exception;
                    activeChunkTask = null;

                    if (faulted)
                    {
                        Debug.LogException(taskException);
                        continue;
                    }

                    Vector3 worldOffset = new Vector3(
                        chunkX * (commonSettings.resolution - 1) * generationScale,
                        0,
                        chunkY * (commonSettings.resolution - 1) * generationScale
                    );

                    GameObject instance = Instantiate(
                        generationChunkObject,
                        worldOffset,
                        Quaternion.identity,
                        transform
                    );
                    spawnedChunks.Add(instance);

                    if (instance.TryGetComponent<Chunk>(out var chunkComp))
                    {
                        chunkComp.generateMesh(
                            heightData,
                            commonSettings.resolution,
                            commonSettings.resolution,
                            generationScale,
                            commonSettings.amplitude
                        );
                    }

                    AccumulateTerrainStatistics(heightData, commonSettings.resolution, generationScale);
                    chunkTimer.Stop();
                    totalChunkGenerationSeconds += chunkTimer.Elapsed.TotalSeconds;
                    CompletedChunkCount++;

                    yield return null;
                }
            }

        }
        finally
        {
            generationTimer.Stop();
            if (activeChunkTask == null || activeChunkTask.IsCompleted)
            {
                DestroyGenerators();
            }
            generationCoroutine = null;
            IsGenerating = false;
        }
    }

    private void AccumulateTerrainStatistics(float[] heights, int size, float spacing)
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
                    AddTriangleStatistics(h00, h01, h10, (h10 - h00) / spacing, (h01 - h00) / spacing);
                    AddTriangleStatistics(h10, h01, h11, (h11 - h01) / spacing, (h11 - h10) / spacing);
                }
            }
        }

        AverageHeight = triangleCount > 0 ? triangleHeightSum / triangleCount
            : sampleCount > 0 ? sampleHeightSum / sampleCount : 0.0;
        AverageSlope = triangleCount > 0 ? slopeSum / triangleCount : 0.0;
    }

    private void AddTriangleStatistics(double a, double b, double c, double gradientX, double gradientY)
    {
        double slope = Math.Atan(Math.Sqrt(gradientX * gradientX + gradientY * gradientY)) * (180.0 / Math.PI);
        triangleHeightSum += (a + b + c) / 3.0;
        slopeSum += slope;
        triangleCount++;
        if (slope > MaximumSlope) MaximumSlope = slope;
    }

    private void OnDestroy()
    {
        if (generationCoroutine != null)
        {
            StopCoroutine(generationCoroutine);
            generationCoroutine = null;
        }

        try
        {
            activeChunkTask?.GetAwaiter().GetResult();
        }
        catch (Exception exception)
        {
            Debug.LogException(exception);
        }
        finally
        {
            activeChunkTask = null;
            generationTimer.Stop();
            IsGenerating = false;
            ClearChunks();
            DestroyGenerators();
        }
    }
}
