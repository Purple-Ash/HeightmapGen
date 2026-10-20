using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.InputSystem;

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
        // Every entry point (including R) commits the UI draft before snapshotting.
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
        IsGenerating = true;

        // Generate drains any outstanding native call without blocking Unity.
        // Repeated restarts replace the pending request with the latest settings.
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
        // Capture all settings before yielding. Native buffer sizes, chunk layout,
        // and mesh creation must agree even if serialized fields change later.
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
            // Ensure the coroutine handle is assigned even if initialization fails.
            yield return null;

            // The DLL cannot cancel getChunk. Keep its handles alive until it
            // returns, discard that result, and never schedule another old chunk.
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

            float highest = float.NegativeInfinity;
            float lowest = float.PositiveInfinity;
            bool hasSamples = false;

            for (int i = 0; i < generationChunkCount.x; i++)
            {
                for (int j = 0; j < generationChunkCount.y; j++)
                {
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

                    for (int k = 0; k < heightData.Length; k++)
                    {
                        float h = heightData[k];
                        if (h > highest) highest = h;
                        if (h < lowest) lowest = h;
                    }
                    if (totalSamples > 0)
                    {
                        hasSamples = true;
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

                    if (hasSamples)
                    {
                        UpdateHeightStatistics(highest, lowest);
                    }

                    // Let this chunk actually render before starting the next one.
                    yield return null;
                }
            }

        }
        finally
        {
            // StopCoroutine may run this while the native worker is still using
            // the handles. In that case the next run (or OnDestroy) owns cleanup.
            if (activeChunkTask == null || activeChunkTask.IsCompleted)
            {
                DestroyGenerators();
            }
            generationCoroutine = null;
            IsGenerating = false;
        }
    }

    private void UpdateHeightStatistics(float highest, float lowest)
    {
        HighestHeight = highest;
        LowestHeight = lowest;
        HasHeightStatistics = true;
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
            // Teardown must not unload the plugin while a native call is running.
            activeChunkTask?.GetAwaiter().GetResult();
        }
        catch (Exception exception)
        {
            Debug.LogException(exception);
        }
        finally
        {
            activeChunkTask = null;
            IsGenerating = false;
            ClearChunks();
            DestroyGenerators();
        }
    }
}
