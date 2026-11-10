#include "Generators.h"

#include <array>
#include <bit>
#include <cstddef>
#include <stdexcept>

struct EdgeLayout {
    Edge opposite;
    Vec2Int pointOrigin;
    Vec2Int pointStep;
    Vec2Int inwardStep;
};

constexpr std::array<EdgeLayout, 4> edgeLayoutsByBitIndex{{
    { Edge::South, Vec2Int{ 0, 0 }, Vec2Int{ 1, 0 }, Vec2Int{ 0, 1 } },
    { Edge::West, Vec2Int{ 1, 0 }, Vec2Int{ 0, 1 }, Vec2Int{ -1, 0 } },
    { Edge::North, Vec2Int{ 0, 1 }, Vec2Int{ 1, 0 }, Vec2Int{ 0, -1 } },
    { Edge::East, Vec2Int{ 0, 0 }, Vec2Int{ 0, 1 }, Vec2Int{ 1, 0 } }
}};

constexpr std::array<Vec2Int, 4> cornerOffsetsByBitIndex{{
    Vec2Int{ 1, 0 },
    Vec2Int{ 1, 1 },
    Vec2Int{ 0, 1 },
    Vec2Int{ 0, 0 }
}};

size_t edgeLayoutIndex(Edge edge) {
    return std::countr_zero(static_cast<unsigned int>(edge));
}

size_t cornerOffsetIndex(Corner corner) {
    return std::countr_zero(static_cast<unsigned int>(corner)) - 4;
}

bool isEdgeFinalized(Edge edge, uint8_t flags) {
	return (flags & static_cast<uint8_t>(edge)) != 0;
}

uint8_t markEdgeFinalized(Edge edge, uint8_t flags) {
	return flags | static_cast<uint8_t>(edge);
}

bool areAllEdgesFinalized(uint8_t flags) {
	return (flags & static_cast<uint8_t>(15)) == static_cast<uint8_t>(15);
}

bool isCornerFinalized(Corner corner, uint8_t flags) {
	return (flags & static_cast<uint8_t>(corner)) != 0;
}

uint8_t markCornerFinalized(Corner corner, uint8_t flags) {
	return flags | static_cast<uint8_t>(corner);
}

bool isChunkFullyBlended(uint8_t flags) {
    return flags == static_cast<uint8_t>(255);
}

int32_t chunkCoordsToChunkIdx(Vec2Int point, int32_t resolution) {
    return point.x * resolution + point.y;
}

int32_t chunkCoordsToMaskIdx(int32_t x, int32_t y, int32_t resolution, int32_t radius) {
    // reduced northern edge
	if (y < radius && x < resolution - radius && x >= radius) {
		return radius * resolution + y * (resolution - 2 * radius) + (x - radius);
	}

    // reduced southern edge
	if (y >= resolution - radius && x < resolution - radius && x >= radius) {
		return 2 * radius * (resolution - radius) +
			(y + radius - resolution) * (resolution - 2 * radius) + (x - radius);
	}

    // full western edge
	if (x < radius) {
		return x * resolution + y;
	}

    // full eastern edge
	if (x >= resolution - radius) {
		return radius * (3 * resolution - 4 * radius) +
			(x - resolution + radius) * resolution + y;
	}

    // fail
	return -1;
}

int32_t chunkCoordsToMaskIdx(Vec2Int point, int32_t resolution, int32_t radius) {
    return chunkCoordsToMaskIdx(point.x, point.y, resolution, radius);
}

float smoothstep(float t) {
	return t * t * (3.0f - 2.0f * t);
}

int32_t maskSize(int32_t resolution, int32_t radius) {
    return 4 * radius * (resolution - radius);
}

Edge oppositeEdge(Edge edge) {
    return edgeLayoutsByBitIndex[edgeLayoutIndex(edge)].opposite;
}

Vec2Int pointOnEdge(Edge edge, int32_t i, int32_t resolution) {
    const EdgeLayout& layout = edgeLayoutsByBitIndex[edgeLayoutIndex(edge)];
    return Vec2Int{
        layout.pointOrigin.x * (resolution - 1) + layout.pointStep.x * i,
        layout.pointOrigin.y * (resolution - 1) + layout.pointStep.y * i
    };
}

Vec2Int cornerToChunkCoords(Corner corner, int32_t resolution) {
    const Vec2Int offset = cornerOffsetsByBitIndex[cornerOffsetIndex(corner)];
    return Vec2Int{ offset.x * (resolution - 1), offset.y * (resolution - 1) };
}

Vec2Int stepInward(Vec2Int point, Edge edge, int32_t depth) {
    const Vec2Int step = edgeLayoutsByBitIndex[edgeLayoutIndex(edge)].inwardStep;
    point.x += step.x * depth;
    point.y += step.y * depth;
    return point;
}

float sumInward(const std::vector<float>& chunk, Vec2Int point, Edge edge, int32_t depth, int32_t resolution) {
    float total = chunk[chunkCoordsToChunkIdx(point, resolution)];
    for (int32_t i = 1; i < depth; ++i) {
        Vec2Int newPoint = stepInward(point, edge, i);
        total += chunk[chunkCoordsToChunkIdx(newPoint, resolution)];
    }
    return total;
}

float averageEdgeHeight(const std::vector<float>& target, const std::vector<float>& other, Edge edge, int32_t i, int32_t resolution) {
    return (target[chunkCoordsToChunkIdx(pointOnEdge(edge, i, resolution), resolution)] +
            other[chunkCoordsToChunkIdx(pointOnEdge(oppositeEdge(edge), i, resolution), resolution)])
            / 2.0f;
}

float averageEdgeHeightOverRadius(const std::vector<float>& target, const std::vector<float>& other, Edge edge, int32_t i, int32_t resolution, int32_t radius) {
    Edge opposite = oppositeEdge(edge);
    return (sumInward(target, pointOnEdge(edge, i, resolution), edge, radius, resolution) +
            sumInward(other, pointOnEdge(opposite, i, resolution), opposite, radius, resolution))
            / (2.0f * static_cast<float>(radius));
}

void calculateAndStoreCorrection(const std::vector<float>& chunk, std::vector<float>& mask, Edge edge, Vec2Int initialPoint, int32_t resolution, 
    int32_t radius, int32_t depth, float heightOnEdge, float blendingStrength) {
	Vec2Int currentPoint = stepInward(initialPoint, edge, depth);
	mask[chunkCoordsToMaskIdx(currentPoint, resolution, radius)] += blendingStrength * (heightOnEdge - chunk[chunkCoordsToChunkIdx(currentPoint, resolution)]);
}

void divideCornerAreaBy2(std::vector<float>& mask, Corner corner, uint8_t flags, int32_t resolution, int32_t radius) {
    if (isCornerFinalized(corner, flags))
        return;

    Vec2Int point = cornerToChunkCoords(corner, resolution);
    Edge horizontal = (corner == Corner::SouthEast || corner == Corner::NorthEast) ? Edge::East : Edge::West;
    Edge vertical = (corner == Corner::SouthEast || corner == Corner::SouthWest) ? Edge::South : Edge::North;

    for (int i = 0; i < radius; ++i) {
        Vec2Int pi = stepInward(point, vertical, i);
        for (int j = 0; j < radius; ++j) {
            Vec2Int pj = stepInward(pi, horizontal, j);
            mask[chunkCoordsToMaskIdx(pj, resolution, radius)] /= 2.0f;
        }
    }
}

void standardizeSharedCornerPointHeights(std::vector<float>& northEast, std::vector<float>& southEast, std::vector<float>& southWest, std::vector<float>& northWest, 
    int32_t resolution, int32_t radius) {
    int32_t sw = chunkCoordsToMaskIdx(cornerToChunkCoords(Corner::SouthWest, resolution), resolution, radius);
    int32_t nw = chunkCoordsToMaskIdx(cornerToChunkCoords(Corner::NorthWest, resolution), resolution, radius);
    int32_t ne = chunkCoordsToMaskIdx(cornerToChunkCoords(Corner::NorthEast, resolution), resolution, radius);
    int32_t se = chunkCoordsToMaskIdx(cornerToChunkCoords(Corner::SouthEast, resolution), resolution, radius);

    float avg = (northEast[sw] + southEast[nw] + southWest[ne] + northWest[se]) / 4.0f;

    northEast[sw] = avg;
    southEast[nw] = avg;
    southWest[ne] = avg;
    northWest[se] = avg;
}


SimpleBlenderImpl::SimpleBlenderImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const BlenderSettings& blenderSettings)
	: GeneratorImpl(ctx, commonSettings), blenderSettings(blenderSettings),
	  baseGenerator(ctx->getGenerator(blenderSettings.baseGeneratorImpl)) {
	if (!baseGenerator) {
		throw std::invalid_argument("Blender requires a base generator");
	}
	if (settings.resolution < 2) {
		throw std::invalid_argument("Blender resolution must be at least 2");
	}
	if (blenderSettings.blendingRadius <= 0 ||
		static_cast<int64_t>(blenderSettings.blendingRadius) * 2 > settings.resolution) {
		throw std::invalid_argument("Blender radius must be positive and no greater than half the resolution");
	}
	if (blenderSettings.edgeSamplingRadius <= 0 ||
		blenderSettings.edgeSamplingRadius > settings.resolution) {
		throw std::invalid_argument("Blender edge-sampling radius must be positive and no greater than half the resolution");
	}
	if (baseGenerator->getSettings().resolution != settings.resolution) {
		throw std::invalid_argument("Blender and base generator resolutions must match");
	}
}

std::shared_ptr<SimpleBlenderImpl::BlenderChunkData> SimpleBlenderImpl::getBlenderChunkData(Vec2Int pos) {
    auto it = maskCache.find(pos);
    if (it != maskCache.end()) {
		return it->second;
	}

    auto data = std::make_shared<BlenderChunkData>();
    data->mask.resize(maskSize(settings.resolution, blenderSettings.blendingRadius));
    data->chunk.resize(settings.resolution * settings.resolution);
    baseGenerator->getChunk(pos, data->chunk.data(), true);

    maskCache[pos] = data;
    return data;
}

void SimpleBlenderImpl::calculateCorrectionAlongEdge(std::shared_ptr<SimpleBlenderImpl::BlenderChunkData> target, std::shared_ptr<SimpleBlenderImpl::BlenderChunkData> neighbour, Edge edge) {
    Edge opposite = oppositeEdge(edge);
	int32_t resolution = settings.resolution;
    int32_t radius = blenderSettings.blendingRadius;
	int32_t depth = blenderSettings.edgeSamplingRadius;

    if (isEdgeFinalized(edge, target->flags) && isEdgeFinalized(opposite, neighbour->flags))
        return;

	for (int32_t i = 0; i < resolution; ++i) {
        Vec2Int pointOnTargetChunkEdge = pointOnEdge(edge, i, resolution);
        Vec2Int pointOnNeighbourChunkEdge = pointOnEdge(opposite, i, resolution);
        float sharedHeightOnEdge = averageEdgeHeightOverRadius(target->chunk, neighbour->chunk, edge, i, resolution, depth);

        for (int32_t j = 0; j < radius; ++j) {
            float blendingStrength = smoothstep(1.0f - static_cast<float>(j) / static_cast<float>(radius));
            calculateAndStoreCorrection(target->chunk, target->mask, edge, pointOnTargetChunkEdge, resolution, radius, j, sharedHeightOnEdge, blendingStrength);
            calculateAndStoreCorrection(neighbour->chunk, neighbour->mask, opposite, pointOnNeighbourChunkEdge, resolution, radius, j, sharedHeightOnEdge, blendingStrength);
		}
	}

    target->flags = markEdgeFinalized(edge, target->flags);
    neighbour->flags = markEdgeFinalized(opposite, neighbour->flags);
}

void SimpleBlenderImpl::correctSharedCorner(std::shared_ptr<BlenderChunkData> northEast, std::shared_ptr<BlenderChunkData> southEast, 
    std::shared_ptr<BlenderChunkData> southWest, std::shared_ptr<BlenderChunkData> northWest) {  
    // is the shared corner already finished?
    if (isCornerFinalized(Corner::SouthWest, northEast->flags) &&
        isCornerFinalized(Corner::NorthWest, southEast->flags) &&
        isCornerFinalized(Corner::NorthEast, southWest->flags) &&
        isCornerFinalized(Corner::SouthEast, northWest->flags))
        return;

	const int32_t resolution = settings.resolution;
	const int32_t radius = blenderSettings.blendingRadius;

    divideCornerAreaBy2(northEast->mask, Corner::SouthWest, northEast->flags, resolution, radius);
    divideCornerAreaBy2(southEast->mask, Corner::NorthWest, southEast->flags, resolution, radius);
    divideCornerAreaBy2(southWest->mask, Corner::NorthEast, southWest->flags, resolution, radius);
    divideCornerAreaBy2(northWest->mask, Corner::SouthEast, northWest->flags, resolution, radius);

	standardizeSharedCornerPointHeights(northEast->mask, southEast->mask, southWest->mask, northWest->mask, resolution, radius);

    northEast->flags = markCornerFinalized(Corner::SouthWest, northEast->flags);
    southEast->flags = markCornerFinalized(Corner::NorthWest, southEast->flags);
    southWest->flags = markCornerFinalized(Corner::NorthEast, southWest->flags);
    northWest->flags = markCornerFinalized(Corner::SouthEast, northWest->flags);
}

void SimpleBlenderImpl::mergeChunkWithMask(std::shared_ptr<SimpleBlenderImpl::BlenderChunkData> target, float* buffer) {
    int32_t resolution = settings.resolution;
    int32_t radius = blenderSettings.blendingRadius;

    std::copy(target->chunk.begin(), target->chunk.end(), buffer);

    for (int32_t i = 0; i < resolution; i++)
    {
        for (int32_t j = 0; j < resolution; j++)
        {
            if (i >= radius && i < resolution - radius &&
                j >= radius && j < resolution - radius)
                continue;

            buffer[chunkCoordsToChunkIdx(Vec2Int{ i, j }, resolution)] += target->mask[chunkCoordsToMaskIdx(Vec2Int{ i, j }, resolution, radius)];
        }
    }
}

void SimpleBlenderImpl::generateChunkData(Vec2Int pos, float *buffer) {
	std::lock_guard lock(blenderMutex);

    auto target = getBlenderChunkData(pos);

    if (isChunkFullyBlended(target->flags))
        return;

    auto north = getBlenderChunkData(pos + Vec2Int{ 0, -1 });
    auto northeast = getBlenderChunkData(pos + Vec2Int{ 1, -1 });
    auto east = getBlenderChunkData(pos + Vec2Int{ 1, 0 });
    auto southeast = getBlenderChunkData(pos + Vec2Int{ 1, 1 });
    auto south = getBlenderChunkData(pos + Vec2Int{ 0, 1 });
    auto southwest = getBlenderChunkData(pos + Vec2Int{ -1, 1 });
    auto west = getBlenderChunkData(pos + Vec2Int{ -1, 0 });
    auto northwest = getBlenderChunkData(pos + Vec2Int{ -1, -1 });


    calculateCorrectionAlongEdge(north, northeast, Edge::East);
    calculateCorrectionAlongEdge(north, northwest, Edge::West);

    calculateCorrectionAlongEdge(east, northeast, Edge::North);
    calculateCorrectionAlongEdge(east, southeast, Edge::South);

    calculateCorrectionAlongEdge(south, southeast, Edge::East);
    calculateCorrectionAlongEdge(south, southwest, Edge::West);

    calculateCorrectionAlongEdge(west, southwest, Edge::South);
    calculateCorrectionAlongEdge(west, northwest, Edge::North);

    calculateCorrectionAlongEdge(target, north, Edge::North);
    calculateCorrectionAlongEdge(target, east,  Edge::East);
    calculateCorrectionAlongEdge(target, south, Edge::South);
    calculateCorrectionAlongEdge(target, west,  Edge::West);


	correctSharedCorner(northeast, east, target, north);
	correctSharedCorner(east, southeast, south, target);
	correctSharedCorner(target, south, southwest, west);
	correctSharedCorner(north, target, west, northwest);

    
	mergeChunkWithMask(target, buffer);
}

float SimpleBlenderImpl::getHeight(Vec2Int pos) {
	const Vec2Int chunkPos = pointToChunk(pos);
	const Vec2Int localPos = pos - chunkPos * std::max(settings.resolution - 1, 1);
	std::vector<float> chunk(static_cast<size_t>(settings.resolution) * settings.resolution);
	generateChunkData(chunkPos, chunk.data());
	return chunk[static_cast<size_t>(localPos.x) * settings.resolution + localPos.y];
}

// Blender itself is deterministic, but the baseGenerator might not be
bool SimpleBlenderImpl::isDeterministic() {
    return baseGenerator->isDeterministic();
}

// TODO: manage blender cache
void SimpleBlenderImpl::cleanChunkFromCache(Vec2Int pos) {
	std::lock_guard lock(blenderMutex);
	GeneratorImpl::cleanChunkFromCache(pos);
	maskCache.clear();
}

void SimpleBlenderImpl::clearAllCache() {
	std::lock_guard lock(blenderMutex);
	GeneratorImpl::clearAllCache();
    maskCache.clear();
}