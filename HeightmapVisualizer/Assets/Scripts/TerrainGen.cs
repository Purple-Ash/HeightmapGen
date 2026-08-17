using System;
using System.Runtime.InteropServices;
using UnityEngine;

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


    private IntPtr context = IntPtr.Zero;
    private IntPtr generator = IntPtr.Zero;

    void Start()
    {
        context = HeightmapGenAPI.CreateContext();
        if (context == IntPtr.Zero)
        {
            Debug.LogError("Failed to create HeightmapGen context.");
            return;
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
                generator = HeightmapGenAPI.CreatePerlinGenerator(context, commonSettings);
                break;

            case GeneratorType.BrownianNoise:
                generator = HeightmapGenAPI.CreateBrownianPerlinGenerator(context, commonSettings);
                break;

            case GeneratorType.HydraulicErosion:
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
                generator = HeightmapGenAPI.CreateHydraulicErosionGenerator(context, commonSettings, erosionSettings);
                break;
        }

        if (generator == IntPtr.Zero)
        {
            Debug.LogError("Failed to create Generator instance.");
            return;
        }

        for (int i = 0; i < chunkCount.x; i++)
        {
            for (int j = 0; j < chunkCount.y; j++)
            {
                IntPtr chunkBufferPtr = HeightmapGenAPI.GetChunk(generator, i, j);
                if (chunkBufferPtr == IntPtr.Zero)
                {
                    Debug.LogWarning($"Couldn't get chunk at ({i}, {j})");
                    continue;
                }

                int totalSamples = resolution * resolution;
                float[] heightData = new float[totalSamples];
                Marshal.Copy(chunkBufferPtr, heightData, 0, totalSamples);

                Vector3 worldOffset = new Vector3(
                    i * (resolution - 1) * scale,
                    0,
                    j * (resolution - 1) * scale
                );

                GameObject instance = Instantiate(
                    chunkObject,
                    worldOffset,
                    Quaternion.identity
                );

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
            }
        }
    }

    private void OnDestroy()
    {
        if (generator != IntPtr.Zero)
        {
            HeightmapGenAPI.DestroyGenerator(generator);
            generator = IntPtr.Zero;
        }

        if (context != IntPtr.Zero)
        {
            HeightmapGenAPI.DestroyContext(context);
            context = IntPtr.Zero;
        }
    }
}