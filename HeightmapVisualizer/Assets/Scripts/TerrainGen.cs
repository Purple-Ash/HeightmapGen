using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class TerrainGen : MonoBehaviour
{
    [SerializeField] GameObject chunkObject;
    IntPtr context;
    [SerializeField] Vector2Int chunkDimensions;
    [SerializeField] Vector2Int chunkCount;
    [SerializeField] int resolution;
    [SerializeField] float scale;
    [SerializeField] float amplitude;

    void Start()
    {
        context = HeightmapGenAPI.getNewContext();

        if (!HeightmapGenAPI.setGenerator(context, HeightmapGenAPI.GeneratorType.perlin, scale / 100f))
        {
            Debug.Log("Couldnt set generator");
            return;
        }

        if (!HeightmapGenAPI.setRegionDimensions(context, chunkDimensions.x, chunkDimensions.y))
        {
            Debug.Log("Couldnt set dimensions");
            return;
        }

        if (!HeightmapGenAPI.setResolution(context, resolution))
        {
            Debug.Log("Couldnt set resolution");
            return;
        }

        if (!HeightmapGenAPI.setVerticalAmplitude(context, amplitude))
        {
            Debug.Log("Couldnt set amplitude");
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

                int length = (chunk.size.x + 1) * (chunk.size.y + 1) * resolution * resolution;
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
