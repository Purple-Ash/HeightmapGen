using System;
using System.Runtime.InteropServices;
using UnityEngine;

public static class HeightmapGenAPI
{
    private const string DLL_NAME = "HeightmapGen";

    // Matches Vec2Int in your C++ code
    [StructLayout(LayoutKind.Sequential)]
    public struct Vec2Int
    {
        public int x;
        public int y;
    }

    // Matches the Chunk struct in your C++ code
    // Note: Use this only if you intend to marshal the full struct
    [StructLayout(LayoutKind.Sequential)]
    public struct Chunk
    {
        public IntPtr data; // int* in C++
        public Vec2Int size;
    }

    public enum GeneratorType
    {
        empty,
        random,
        generatorTypeSize
    }

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr smokeTest([MarshalAs(UnmanagedType.LPStr)] string data);


    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr getNewContext();

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool closeContext(IntPtr ctx);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool setRegionDimensions(IntPtr ctx, int sizeX, int sizeY);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool setGenerator(IntPtr ctx, GeneratorType generator_type, float scaleHorizontal, float scaleVertical);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool generateRegion(IntPtr ctx, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool getRegion(IntPtr ctx, int x, int y, out IntPtr chunkPtr);


}