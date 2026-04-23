using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class TerrainGen : MonoBehaviour
{
    [SerializeField] GameObject defaultCube;
    IntPtr context;

    void Start()
    {
        context = HeightmapGenAPI.getNewContext();

        if(!HeightmapGenAPI.setGenerator(context, HeightmapGenAPI.GeneratorType.random, 1.0f, 1.0f))
        {
            Debug.Log("Couldnt set generator");
            return;
        }

        if (!HeightmapGenAPI.generateRegion(context, 1, 1))
        {
            Debug.Log("Couldnt generate region");
            return;
        }

        if (!HeightmapGenAPI.getRegion(context, 1, 1, out IntPtr chunkPtr))
        {
            Debug.Log("Couldnt get region");
            return;
        }

        var chunk = Marshal.PtrToStructure<HeightmapGenAPI.Chunk>(chunkPtr);
        Debug.Log($"Chunk Size: {chunk.size.x}x{chunk.size.y}");

        int length = chunk.size.x * chunk.size.y;
        float[] heightData = new float[length];
        Marshal.Copy(chunk.data, heightData, 0, length);
        for (int i = 0; i < chunk.size.x; i++)
        {
            for(int j = 0; j < chunk.size.y; j++)
            {
                Instantiate(defaultCube, new Vector3(i, heightData[i + j * chunk.size.x],j), Quaternion.identity);
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
