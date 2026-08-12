using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class TerrainGen : MonoBehaviour
{
    [Header("Terrain")]
    [SerializeField] private GameObject chunkObject;
    [SerializeField] private Vector2Int chunkDimensions = new Vector2Int(16, 16);
    [SerializeField] private Vector2Int chunkCount = new Vector2Int(1, 1);
    [Min(1)] [SerializeField] private int resolution = 1;
    [Min(0.0001f)] [SerializeField] private float scale = 2.0f;
    [SerializeField] private float amplitude = 5.0f;
    [SerializeField] private HeightmapGenAPI.GeneratorType generator =
        HeightmapGenAPI.GeneratorType.brownian_perlin;

    [Header("Hydraulic Erosion")]
    [SerializeField] private int erosionSeed;
    [Min(0)] [SerializeField] private int erosionIterations = 1;
    [Min(1)] [SerializeField] private int erosionRadius = 3;
    [Min(1)] [SerializeField] private int maxDropletLifetime = 30;
    [Range(0.0f, 1.0f)] [SerializeField] private float inertia = 0.05f;
    [Min(0.0f)] [SerializeField] private float sedimentCapacityFactor = 4.0f;
    [Min(0.0f)] [SerializeField] private float minSedimentCapacity = 0.01f;
    [Range(0.0f, 1.0f)] [SerializeField] private float erodeSpeed = 0.3f;
    [Range(0.0f, 1.0f)] [SerializeField] private float depositSpeed = 0.3f;
    [Range(0.0f, 1.0f)] [SerializeField] private float evaporateSpeed = 0.01f;
    [Min(0.0f)] [SerializeField] private float gravity = 4.0f;
    [Min(0.0f)] [SerializeField] private float initialSpeed = 1.0f;
    [Min(0.0f)] [SerializeField] private float initialWaterVolume = 1.0f;

    IntPtr context;

    void Start()
    {
        context = HeightmapGenAPI.getNewContext();

        if (!HeightmapGenAPI.setRegionDimensions(context, chunkDimensions.x, chunkDimensions.y))
        {
            Debug.Log("Couldnt set dimensions");
            return;
        }

        if (!HeightmapGenAPI.setVerticalAmplitude(context, amplitude))
        {
            Debug.Log("Couldnt set amplitude");
            return;
        }

        if (!HeightmapGenAPI.setResolution(context, resolution))
        {
            Debug.Log("Couldnt set resolution");
            return;
        }

        bool generatorConfigured;
        if (generator == HeightmapGenAPI.GeneratorType.hydraulic_erosion)
        {
            HeightmapGenAPI.HydraulicErosionSettings erosionSettings =
                HeightmapGenAPI.HydraulicErosionSettings.Default;
            erosionSettings.seed = erosionSeed;
            erosionSettings.numIterations = erosionIterations;
            erosionSettings.erosionRadius = erosionRadius;
            erosionSettings.maxDropletLifetime = maxDropletLifetime;
            erosionSettings.inertia = inertia;
            erosionSettings.sedimentCapacityFactor = sedimentCapacityFactor;
            erosionSettings.minSedimentCapacity = minSedimentCapacity;
            erosionSettings.erodeSpeed = erodeSpeed;
            erosionSettings.depositSpeed = depositSpeed;
            erosionSettings.evaporateSpeed = evaporateSpeed;
            erosionSettings.gravity = gravity;
            erosionSettings.initialSpeed = initialSpeed;
            erosionSettings.initialWaterVolume = initialWaterVolume;

            generatorConfigured = HeightmapGenAPI.setGenerator(
                context,
                generator,
                scale / 100f,
                ref erosionSettings);
        }
        else
        {
            generatorConfigured = HeightmapGenAPI.setGenerator(context, generator, scale / 100f);
        }

        if (!generatorConfigured)
        {
            Debug.LogError("Couldn't set generator");
            return;
        }

        for (int i = 0; i < chunkCount.x; i++)
        {
            for (int j = 0; j < chunkCount.y; j++)
            {
                if (!HeightmapGenAPI.generateRegion(context, i, j))
                {
                    Debug.Log("Couldnt generate region");
                    return;
                }

                if (!HeightmapGenAPI.getRegion(context, i, j, out IntPtr chunkPtr))
                {
                    Debug.Log("Couldnt get region");
                    return;
                }

                var chunk = Marshal.PtrToStructure<HeightmapGenAPI.Chunk>(chunkPtr);
                Debug.Log($"Chunk Size: {chunk.size.x}x{chunk.size.y}");

                int length = (chunk.size.x * resolution + 1) * (chunk.size.y * resolution + 1);
                float[] heightData = new float[length];
                Marshal.Copy(chunk.data, heightData, 0, length);
                GameObject instance = Instantiate(
                    chunkObject, 
                    new Vector3(chunkDimensions.x * i, 0, chunkDimensions.y * j), 
                    Quaternion.identity
                );

                instance.GetComponent<Chunk>().generateMesh(
                    heightData, 
                    chunk.size.x, 
                    chunk.size.y, 
                    resolution,amplitude
                );
            }
        }

    }

    private void OnDestroy()
    {
        if (context != IntPtr.Zero)
        {
            HeightmapGenAPI.closeContext(context);
            context = IntPtr.Zero;
        }
    }
}
