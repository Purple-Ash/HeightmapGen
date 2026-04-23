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
	generatorTypeSize
};

struct Chunk;
struct Vec2Int;

extern "C" {
	HG_API const char* smokeTest(const char* data);

	HG_API HGContextHandle getNewContext();
	HG_API bool closeContext(HGContextHandle ctx);

	HG_API bool setRegionDimensions(HGContextHandle ctx, int32_t sizeX, int32_t sizeY);
	HG_API bool setGenerator(HGContextHandle ctx, GeneratorType generator_type, float scaleHorizontal, float scaleVertical);

	HG_API bool generateRegion(HGContextHandle ctx, int32_t x, int32_t y);
	HG_API bool getRegion(HGContextHandle ctx, int32_t x, int32_t y, Chunk*& data);
}
