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
        public float amplitude;
    }

    public enum GeneratorType : int
    {
        empty = 0,
        random = 1,
        perlin = 2,
        brownian_perlin = 3,
        hydraulic_erosion = 4,
        coordinate = 5
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct HydraulicErosionSettings
    {
        public int seed;
        public int numIterations;
        public int erosionRadius;
        public int maxDropletLifetime;
        public float inertia;
        public float sedimentCapacityFactor;
        public float minSedimentCapacity;
        public float erodeSpeed;
        public float depositSpeed;
        public float evaporateSpeed;
        public float gravity;
        public float initialSpeed;
        public float initialWaterVolume;

        public static HydraulicErosionSettings Default
        {
            get
            {
                return new HydraulicErosionSettings
                {
                    seed = 0,
                    numIterations = 10,
                    erosionRadius = 3,
                    maxDropletLifetime = 30,
                    inertia = 0.05f,
                    sedimentCapacityFactor = 4.0f,
                    minSedimentCapacity = 0.01f,
                    erodeSpeed = 0.3f,
                    depositSpeed = 0.3f,
                    evaporateSpeed = 0.01f,
                    gravity = 4.0f,
                    initialSpeed = 1.0f,
                    initialWaterVolume = 1.0f
                };
            }
        }
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

    [DllImport(DLL_NAME, EntryPoint = "setGenerator", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    private static extern bool setGeneratorWithoutSettings(
        IntPtr ctx,
        GeneratorType generator_type,
        float scaleHorizontal,
        IntPtr settings
    );

    [DllImport(DLL_NAME, EntryPoint = "setGenerator", CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.U1)]
    private static extern bool setGeneratorWithHydraulicSettings(
        IntPtr ctx,
        GeneratorType generator_type,
        float scaleHorizontal,
        ref HydraulicErosionSettings settings
    );

    public static bool setGenerator(
        IntPtr ctx,
        GeneratorType generatorType,
        float scaleHorizontal)
    {
        return setGeneratorWithoutSettings(ctx, generatorType, scaleHorizontal, IntPtr.Zero);
    }

    public static bool setGenerator(
        IntPtr ctx,
        GeneratorType generatorType,
        float scaleHorizontal,
        ref HydraulicErosionSettings settings)
    {
        return setGeneratorWithHydraulicSettings(ctx, generatorType, scaleHorizontal, ref settings);
    }

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
