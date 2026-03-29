#pragma once
#include <cstdint>
#include <unordered_map>

#ifdef _WIN32
#ifdef BUILDING_HG_DLL
#define HG_API __declspec(dllexport)
#else
#define HG_API __declspec(dllimport)
#endif
#else
#define HG_API
#endif


class HGContext;
typedef HGContext* HGContextHandle;

enum struct GeneratorType {
	empty,
	random,
	generatorTypeSize
};

struct Chunk;
struct Vec2Int;

extern "C" {
	const char* smokeTest(const char* data);

	HGContextHandle getNewContext();
	bool closeContext(HGContextHandle ctx);

	bool setRegionDimensions(HGContextHandle ctx, int32_t sizeX, int32_t sizeY);
	bool setGenerator(HGContextHandle ctx, GeneratorType generator_type, float scaleHorizontal, float scaleVertical);

	bool generateRegion(HGContextHandle ctx, int32_t x, int32_t y);
	bool getRegion(HGContextHandle ctx, int32_t x, int32_t y, Chunk*& data);
}
