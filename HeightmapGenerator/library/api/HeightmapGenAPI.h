#pragma once
#include <cstdint>
#include "onnxruntime_cxx_api.h"

#ifdef _WIN32
#ifdef HeightmapGen_EXPORTS
#define HG_API __declspec(dllexport)
#else
#define HG_API __declspec(dllimport)
#endif
#else
#define HG_API
#endif

typedef struct ContextImpl* Context;
typedef struct GeneratorImpl* GeneratorHandle;

struct CommonSettings
{
    uint64_t seed;
    float scale;
    float amplitude;
    int32_t resolution;
    bool cacheable;
};

struct HydraulicErosionSettings
{
    int32_t seed = 0;
    int32_t numIterations = 10;
    int32_t erosionRadius = 3;
    int32_t maxDropletLifetime = 30;
    float inertia = 0.05f;
    float sedimentCapacityFactor = 4.0f;
    float minSedimentCapacity = 0.01f;
    float erodeSpeed = 0.3f;
    float depositSpeed = 0.3f;
    float evaporateSpeed = 0.01f;
    float gravity = 4.0f;
    float initialSpeed = 1.0f;
    float initialWaterVolume = 1.0f;
	GeneratorHandle baseGeneratorImpl = nullptr;
};

extern "C" {
    HG_API const char* smokeTest(const char* data);

    HG_API Context CreateContext();
    HG_API void DestroyContext(Context ctx);

    HG_API GeneratorHandle CreateHydraulicErosionGenerator(Context ctx, CommonSettings commonSettings, HydraulicErosionSettings erosionSettings);
    HG_API GeneratorHandle CreatePerlinGenerator(Context ctx, CommonSettings commonSettings);
    HG_API GeneratorHandle CreateBrownianPerlinGenerator(Context ctx, CommonSettings commonSettings);
    HG_API GeneratorHandle CreateBPGenerator(Context ctx, CommonSettings commonSettings, const Ort::Env &env, const char* modelPath, const Ort::SessionOptions &sessionOptions = Ort::SessionOptions{nullptr});
    HG_API void DestroyGenerator(GeneratorHandle GeneratorImpl);

    HG_API float* GetChunk(GeneratorHandle GeneratorImpl, int32_t x, int32_t y);
    HG_API float GetPoint(GeneratorHandle GeneratorImpl, int32_t x, int32_t y);

    HG_API void RequestChunk(GeneratorHandle GeneratorImpl, int32_t x, int32_t y);
    HG_API void RequestPoint(GeneratorHandle GeneratorImpl, int32_t x, int32_t y);
    HG_API float* ProbeChunk(GeneratorHandle GeneratorImpl, int32_t x, int32_t y, bool* ready);
    HG_API float ProbePoint(GeneratorHandle GeneratorImpl, int32_t x, int32_t y, bool* ready);

    HG_API void CleanChunkFromCache(GeneratorHandle GeneratorImpl, int32_t x, int32_t y);
    HG_API void ClearAllCache(GeneratorHandle GeneratorImpl);
}