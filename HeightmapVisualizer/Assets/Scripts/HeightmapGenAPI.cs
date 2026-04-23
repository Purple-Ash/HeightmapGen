using System;
using System.Runtime.InteropServices;

public static class HeightmapGenAPI
{
    private const string DLL_NAME = "HeightmapGen";

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec2Int
    {
        public int x;
        public int y;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Chunk
    {
        public IntPtr data;
        public Vec2Int size;
        public int resolution;
    }
    public enum GeneratorType : int
    {
        empty = 0,
        random = 1,
        perlin = 2,
        generatorTypeSize = 3
    }

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr smokeTest(
        [MarshalAs(UnmanagedType.LPStr)] string data
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr getNewContext();

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool closeContext(IntPtr ctx);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool setRegionDimensions(
        IntPtr ctx,
        int sizeX,
        int sizeY
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool setVerticalAmplitude(
        IntPtr ctx,
        float multiplier
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool setResolution(
        IntPtr ctx,
        int resolution
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool setGenerator(
        IntPtr ctx,
        GeneratorType generator_type,
        float scaleHorizontal
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool generateRegion(
        IntPtr ctx,
        int x,
        int y
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static extern bool getRegion(
        IntPtr ctx,
        int x,
        int y,
        out IntPtr chunkPtr
    );

    public static Chunk PtrToChunk(IntPtr ptr)
    {
        return Marshal.PtrToStructure<Chunk>(ptr);
    }

    public static string PtrToString(IntPtr ptr)
    {
        return Marshal.PtrToStringAnsi(ptr);
    }
}