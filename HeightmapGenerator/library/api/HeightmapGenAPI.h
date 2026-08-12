#pragma once
#include <cstdint>
#include <unordered_map>

#ifdef _WIN32
#ifdef HeightmapGen_EXPORTS
#define HG_API __declspec(dllexport)
#else
#define HG_API __declspec(dllimport)
#endif
#else
#define HG_API
#endif

class HGContext;
using HGContextHandle = HGContext*;

enum struct GeneratorType {
	empty = 0,
	random = 1,
	perlin = 2,
	brownian_perlin = 3,
	hydraulic_erosion = 4,
	coordinate = 5,
	generatorTypeSize
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
};

struct Chunk;
struct Vec2Int;

extern "C" {
	HG_API const char* smokeTest(const char* data);

	HG_API HGContextHandle getNewContext();
	HG_API bool closeContext(HGContextHandle ctx);

	HG_API bool setRegionDimensions(HGContextHandle ctx, int32_t sizeX, int32_t sizeY);
	HG_API bool setVerticalAmplitude(HGContextHandle ctx, float multiplier);
	HG_API bool setResolution(HGContextHandle ctx, int32_t resolution);
	HG_API bool setGenerator(HGContextHandle ctx, GeneratorType generator_type, float scaleHorizontal, void* settings);

	HG_API bool generateRegion(HGContextHandle ctx, int32_t x, int32_t y);
	HG_API bool getRegion(HGContextHandle ctx, int32_t x, int32_t y, Chunk*& data);
}




