#pragma once

#include "Generators.h"
#include <array>
#include <unordered_set>

class BlenderImpl {
	enum class Edge {
		North,
		East,
		South,
		West
	};

	enum class Corner {
		NorthEast,
		SouthEast,
		SouthWest,
		NorthWest
	};

	struct ChunkData {
		std::vector<float> heights;
		std::vector<float> mask;
		bool eastEdgeProcessed = false;
		bool southEdgeProcessed = false;
	};

	struct CornerSample {
		ChunkData* chunk;
		int32_t x;
		int32_t y;
		int32_t regionX;
		int32_t regionY;
	};

	std::shared_ptr<GeneratorImpl> source;
	CommonSettings settings;
	int32_t radius;
	std::mutex maskMutex;
	std::unordered_map<Vec2Int, ChunkData> maskCache;
	std::unordered_set<Vec2Int> processedCorners;

	ChunkData& getChunkData(Vec2Int pos);
	void smoothEdge(ChunkData& target, ChunkData& neighbour, Edge edge);
	void correctCorner(const std::array<CornerSample, 4>& samples, Vec2Int vertex);
	void correctSharedBoundaries();

public:
	BlenderImpl(std::shared_ptr<GeneratorImpl> source, int32_t radius);

	void getChunk(Vec2Int chunkPos, float* buffer);
	void clearAllCache();
};
