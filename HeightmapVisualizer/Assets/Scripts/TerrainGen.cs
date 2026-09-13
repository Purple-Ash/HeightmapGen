using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading.Tasks;
using TMPro;
using UnityEngine;
using UnityEngine.InputSystem;

public class TerrainGen : MonoBehaviour
{
    [SerializeField] private GameObject highestPoint;
    [SerializeField] private GameObject lowestPoint;
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

    private IntPtr context = IntPtr.Zero;
    private IntPtr generator = IntPtr.Zero;
    private IntPtr baseGenerator = IntPtr.Zero;

    private readonly List<GameObject> spawnedChunks = new List<GameObject>();
    private Coroutine generationCoroutine;
    private Task activeChunkTask;

    void Start()
    {
        generationCoroutine = StartCoroutine(Generate());
    }

    void Update()
    {
        var keyboard = Keyboard.current;
        if (keyboard != null && keyboard[Key.R].wasPressedThisFrame)
        {
            ReloadTerrain();
        }
    }

    private void ReloadTerrain()
    {
        if (generationCoroutine != null)
        {
            StopCoroutine(generationCoroutine);
            generationCoroutine = null;
        }

        activeChunkTask?.Wait();
        activeChunkTask = null;
        IsGenerating = false;

        ClearChunks();
        DestroyGenerators();

        generationCoroutine = StartCoroutine(Generate());
    }

    private void ClearChunks()
    {
        foreach (var chunkGO in spawnedChunks)
        {
            if (chunkGO != null)
            {
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
        IsGenerating = true;

        context = HeightmapGenAPI.createContext();
        if (context == IntPtr.Zero)
        {
            Debug.LogError("Failed to create HeightmapGen context.");
            generationCoroutine = null;
            IsGenerating = false;
            yield break;
        }

        HeightmapGenAPI.CommonSettings commonSettings = new HeightmapGenAPI.CommonSettings
        {
            seed = seed,
            scale = scale / 100f,
            amplitude = amplitude,
            resolution = resolution,
            cacheable = cacheable
        };

        switch (generatorType)
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
                    generationCoroutine = null;
                    IsGenerating = false;
                    yield break;
                }

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
                    initialWaterVolume = initialWaterVolume,
                    baseGeneratorImpl = baseGenerator
                };
                generator = HeightmapGenAPI.createHydraulicErosionGenerator(context, commonSettings, erosionSettings);
                break;
        }

        if (generator == IntPtr.Zero)
        {
            Debug.LogError("Failed to create Generator instance");
            generationCoroutine = null;
            IsGenerating = false;
            yield break;
        }

        float highest = float.NegativeInfinity;
        float lowest = float.PositiveInfinity;
        bool hasSamples = false;

        for (int i = 0; i < chunkCount.x; i++)
        {
            for (int j = 0; j < chunkCount.y; j++)
            {
                int chunkX = i - chunkCount.x / 2;
                int chunkY = j - chunkCount.y / 2;
                int totalSamples = resolution * resolution;
                float[] heightData = new float[totalSamples];

                activeChunkTask = Task.Run(() =>
                {
                    HeightmapGenAPI.getChunk(generator, chunkX, chunkY, heightData);
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
                    chunkX * (resolution - 1) * scale,
                    0,
                    chunkY * (resolution - 1) * scale
                );

                GameObject instance = Instantiate(
                    chunkObject,
                    worldOffset,
                    Quaternion.identity,
                    transform
                );
                spawnedChunks.Add(instance);

                if (instance.TryGetComponent<Chunk>(out var chunkComp))
                {
                    chunkComp.generateMesh(
                        heightData,
                        resolution,
                        resolution,
                        scale,
                        amplitude
                    );
                }

                if (hasSamples)
                {
                    UpdateHeightLabels(highest, lowest);
                }

                // Let this chunk actually render before starting the next one.
                yield return null;
            }
        }

        generationCoroutine = null;
        IsGenerating = false;
    }

    private void UpdateHeightLabels(float highest, float lowest)
    {
        if (highestPoint != null)
        {
            highestPoint.GetComponent<TextMeshProUGUI>().text = highest.ToString("F2");
        }

        if (lowestPoint != null)
        {
            lowestPoint.GetComponent<TextMeshProUGUI>().text = lowest.ToString("F2");
        }
    }

    private void OnDestroy()
    {
        if (generationCoroutine != null)
        {
            StopCoroutine(generationCoroutine);
            generationCoroutine = null;
        }

        activeChunkTask?.Wait();
        activeChunkTask = null;
        IsGenerating = false;

        DestroyGenerators();
    }
}
