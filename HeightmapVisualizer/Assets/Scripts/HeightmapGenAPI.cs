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
    public struct BarSettings
    {
        public float mountiness;
        public float continentality;
        public float erosion;
        public float weirdness;
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
            initialWaterVolume = 1.0f
        };
    }

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr smokeTest([MarshalAs(UnmanagedType.LPStr)] string data);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr CreateContext();

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void DestroyContext(IntPtr ctx);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr CreateBarGenerator(IntPtr ctx, CommonSettings commonSettings, BarSettings barSettings);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr CreateHydraulicErosionGenerator(IntPtr ctx, CommonSettings commonSettings, HydraulicErosionSettings erosionSettings);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void DestroyGenerator(IntPtr generator);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr GetChunk(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern float GetPoint(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void RequestChunk(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void RequestPoint(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ProbeChunk(IntPtr generator, int x, int y, out bool ready);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern float ProbePoint(IntPtr generator, int x, int y, out bool ready);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void CleanChunkFromCache(IntPtr generator, int x, int y);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ClearAllCache(IntPtr generator);

    public static string PtrToString(IntPtr ptr)
    {
        return Marshal.PtrToStringAnsi(ptr);
    }
}