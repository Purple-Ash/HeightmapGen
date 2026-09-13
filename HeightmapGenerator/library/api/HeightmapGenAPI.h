#pragma once
#include <cstdint>

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
typedef struct GeneratorImpl* Generator;

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
	Generator baseGeneratorImpl = nullptr;
};

extern "C" {
    HG_API const char* smokeTest(const char* data);

    HG_API Context createContext();
    HG_API void destroyContext(Context ctx);

    HG_API Generator createHydraulicErosionGenerator(Context ctx, CommonSettings commonSettings, HydraulicErosionSettings erosionSettings);
    HG_API Generator createPerlinGenerator(Context ctx, CommonSettings commonSettings);
    HG_API Generator createBrownianPerlinGenerator(Context ctx, CommonSettings commonSettings);
    HG_API void destroyGenerator(Generator generator);

    HG_API void getChunk(Generator generator, int32_t x, int32_t y, float* buffer);
    HG_API float getPoint(Generator generator, int32_t x, int32_t y);
    HG_API void requestChunk(Generator generator, int32_t x, int32_t y);
    HG_API void requestPoint(Generator generator, int32_t x, int32_t y);
    HG_API bool probeChunk(Generator generator, int32_t x, int32_t y, float* buffer);
    HG_API bool probePoint(Generator generator, int32_t x, int32_t y, float* point);

    HG_API void cleanChunkFromCache(Generator generator, int32_t x, int32_t y);
    HG_API void clearAllCache(Generator generator);
}