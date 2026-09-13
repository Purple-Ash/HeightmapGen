using System;
using System.Runtime.InteropServices;

public static class HeightmapGenAPI
{
    private const string DLL_NAME = "HeightmapGen";

    [StructLayout(LayoutKind.Sequential)]
    public struct CommonSettings
    {
        public ulong seed;
        public float scale;
        public float amplitude;
        public int resolution;
        [MarshalAs(UnmanagedType.I1)]
        public bool cacheable;
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
        public IntPtr baseGeneratorImpl;

        public static HydraulicErosionSettings Default => new HydraulicErosionSettings
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
            initialWaterVolume = 1.0f,
            baseGeneratorImpl = IntPtr.Zero
        };
    }

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr smokeTest([MarshalAs(UnmanagedType.LPStr)] string data);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr createContext();

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void destroyContext(IntPtr ctx);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr createPerlinGenerator(IntPtr ctx, CommonSettings commonSettings);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr createBrownianPerlinGenerator(IntPtr ctx, CommonSettings commonSettings);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr createHydraulicErosionGenerator(IntPtr ctx, CommonSettings commonSettings, HydraulicErosionSettings erosionSettings);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void destroyGenerator(IntPtr generator);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void getChunk(IntPtr generator, int x, int y, [Out] float[] buffer);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern float getPoint(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void requestChunk(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void requestPoint(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool probeChunk(IntPtr generator, int x, int y, [Out] float[] buffer);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool probePoint(IntPtr generator, int x, int y, out float point);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void cleanChunkFromCache(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void clearAllCache(IntPtr generator);

    public static string PtrToString(IntPtr ptr)
    {
        return Marshal.PtrToStringAnsi(ptr);
    }
}
