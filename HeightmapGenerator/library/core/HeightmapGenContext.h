#pragma once
#include "Generators.h"
#include "HeightmapGenAPI.h"

struct Vec2Int
{
	int32_t x;
	int32_t y;

	Vec2Int() = default;
	Vec2Int(int32_t x, int32_t y);
	bool operator==(const Vec2Int& other) const = default;
};

struct Chunk
{
	float* data;
	Vec2Int size;
	float amplitude;

	Chunk() = default;
	Chunk(Vec2Int dimensions, int32_t resolution, float amplitude);
	Chunk(Chunk&& other) noexcept;
	Chunk& operator=(Chunk&& other) noexcept;
	~Chunk();
};

namespace std {
	template<>
	struct hash<Vec2Int> {
		size_t operator()(const Vec2Int& c) const noexcept {
			size_t h1 = hash<int>{}(c.x);
			size_t h2 = hash<int>{}(c.y);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};
}

class HGContext
{
	Generator* generator = nullptr;
	std::unordered_map<Vec2Int, Chunk> chunks;
	Vec2Int chunkDimensions = {16,16};
	int32_t resolution = 1;
	float amplitude = 1;

public:
	bool generateRegion(Vec2Int position);
	bool getRegion(Vec2Int position, Chunk*& data);
	bool setGenerator(GeneratorType type, float scaleHorizontal);
	bool setResolution(int32_t resolution);
	bool setRegionDimensions(Vec2Int dimensions);
	bool setVerticalAmplitude(float amplitude);
	~HGContext();
};

