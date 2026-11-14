#include "Generators.h"

#include <array>
#include <bit>
#include <cstddef>
#include <stdexcept>
#include <limits>

struct EdgeLayout {
    Edge opposite;
    Vec2Int pointOrigin;
    Vec2Int pointStep;
    Vec2Int inwardStep;
};

// lookup table describing how to walk along each edge of a chunk
std::array<EdgeLayout, 4> edgeLayoutsByBitIndex{{
    { Edge::South, Vec2Int{ 0, 0 }, Vec2Int{ 1, 0 }, Vec2Int{ 0, 1 } },
    { Edge::West, Vec2Int{ 1, 0 }, Vec2Int{ 0, 1 }, Vec2Int{ -1, 0 } },
    { Edge::North, Vec2Int{ 0, 1 }, Vec2Int{ 1, 0 }, Vec2Int{ 0, -1 } },
    { Edge::East, Vec2Int{ 0, 0 }, Vec2Int{ 0, 1 }, Vec2Int{ 1, 0 } }
}};

// describes
struct EdgeCornerLayout {
    Corner firstStart;
    Corner firstEnd;
    Corner secondStart;
    Corner secondEnd;
};

// lookup table that specifies which corners meet at each end of a shared edge
std::array<EdgeCornerLayout, 4> edgeCornerLayoutsByBitIndex{{
    { Corner::NorthWest, Corner::NorthEast, Corner::SouthWest, Corner::SouthEast },
    { Corner::NorthEast, Corner::SouthEast, Corner::NorthWest, Corner::SouthWest },
    { Corner::SouthWest, Corner::SouthEast, Corner::NorthWest, Corner::NorthEast },
    { Corner::NorthWest, Corner::SouthWest, Corner::NorthEast, Corner::SouthEast }
}};

// lookup table used for quickly translating enumerated corners into chunk coordinates
std::array<Vec2Int, 4> cornerOffsetsByBitIndex{{
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
    return flags == std::numeric_limits<uint8_t>::max();
}

int32_t chunkCoordsToMaskIdx(Vec2Int point, int32_t resolution, int32_t radius) {
    // reduced northern edge
	if (point.y < radius && point.x < resolution - radius && point.x >= radius) {
		return radius * resolution + point.y * (resolution - 2 * radius) + (point.x - radius);
	}

    // reduced southern edge
	if (point.y >= resolution - radius && point.x < resolution - radius && point.x >= radius) {
		return 2 * radius * (resolution - radius) + (point.y + radius - resolution) * (resolution - 2 * radius) + (point.x - radius);
	}

    // full western edge
	if (point.x < radius) {
		return point.x * resolution + point.y;
	}

    // full eastern edge
	if (point.x >= resolution - radius) {
		return radius * (3 * resolution - 4 * radius) +
			(point.x - resolution + radius) * resolution + point.y;
	}

    // fail
	return -1;
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
    EdgeLayout& layout = edgeLayoutsByBitIndex[edgeLayoutIndex(edge)];
    return Vec2Int{
        layout.pointOrigin.x * (resolution - 1) + layout.pointStep.x * i,
        layout.pointOrigin.y * (resolution - 1) + layout.pointStep.y * i
    };
}

Vec2Int cornerToChunkCoords(Corner corner, int32_t resolution) {
    Vec2Int offset = cornerOffsetsByBitIndex[cornerOffsetIndex(corner)];
    return Vec2Int{ offset.x * (resolution - 1), offset.y * (resolution - 1) };
}

Vec2Int stepInward(Vec2Int point, Edge edge, int32_t depth) {
    Vec2Int step = edgeLayoutsByBitIndex[edgeLayoutIndex(edge)].inwardStep;
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

float sumCornerSamples(const std::vector<float>& chunk, Corner corner, int32_t resolution, int32_t radius) {
    Vec2Int point = cornerToChunkCoords(corner, resolution);
    point.x = (point.x == 0) ? 0 : resolution - radius;
    point.y = (point.y == 0) ? 0 : resolution - radius;
    float sum = 0.0f;
    for (int32_t i = 0; i < radius; ++i) {
        for (int32_t j = 0; j < radius; ++j) {
            sum += chunk[(point.x + i) * resolution + point.y + j];
        }
    }
    return sum;
}

void setCornerCorrection(const std::vector<float>& chunk, std::vector<float>& mask, Corner corner, float sharedHeight, int32_t resolution, int32_t radius) {
	Vec2Int point = cornerToChunkCoords(corner, resolution);
	mask[chunkCoordsToMaskIdx(point, resolution, radius)] = sharedHeight - chunk[chunkCoordsToChunkIdx(point, resolution)];
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
	if (blenderSettings.blendingRadius <= 0 || blenderSettings.blendingRadius * 2 > settings.resolution) {
		throw std::invalid_argument("Blender radius must be positive and no greater than half the resolution");
	}
	if (blenderSettings.edgeSamplingRadius <= 0 || blenderSettings.edgeSamplingRadius > settings.resolution) {
		throw std::invalid_argument("Blender edge-sampling radius must be positive and no greater than half the resolution");
	}
	if (baseGenerator->getSettings().resolution != settings.resolution) {
		throw std::invalid_argument("Blender and base generator resolutions must match");
	}
}

void SimpleBlenderImpl::requestChunk(Vec2Int pos, bool forceSingleThreaded) {
    for (int32_t x = -1; x <= 1; x++) {
        for (int32_t y = -1; y <= 1; y++) {
            baseGenerator->requestChunk(pos + Vec2Int{ x, y }, forceSingleThreaded);
        }
    }

    GeneratorImpl::requestChunk(pos, forceSingleThreaded);
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

void SimpleBlenderImpl::correctSharedBoundary(std::shared_ptr<BlenderChunkData> target, std::shared_ptr<BlenderChunkData> neighbour, Edge edge) {
	int32_t resolution = settings.resolution;
	int32_t radius = blenderSettings.blendingRadius;
	Edge opposite = oppositeEdge(edge);
	EdgeCornerLayout cornerLayout = edgeCornerLayoutsByBitIndex[edgeLayoutIndex(edge)];

	for (int32_t i = 0; i < resolution; ++i) {
		Corner firstCorner = (i == 0) ? cornerLayout.firstStart : cornerLayout.firstEnd;
		Corner secondCorner = (i == 0) ? cornerLayout.secondStart : cornerLayout.secondEnd;
		if ((i == 0 || i == resolution - 1) &&
			isCornerFinalized(firstCorner, target->flags) &&
			isCornerFinalized(secondCorner, neighbour->flags)) {
			continue;
		}

		Vec2Int targetPoint = pointOnEdge(edge, i, resolution);
		Vec2Int neighbourPoint = pointOnEdge(opposite, i, resolution);
		float average = averageEdgeHeightOverRadius(target->chunk, neighbour->chunk, edge, i, resolution, blenderSettings.edgeSamplingRadius);

		target->mask[chunkCoordsToMaskIdx(targetPoint, resolution, radius)] = average - target->chunk[chunkCoordsToChunkIdx(targetPoint, resolution)];
		neighbour->mask[chunkCoordsToMaskIdx(neighbourPoint, resolution, radius)] = average - neighbour->chunk[chunkCoordsToChunkIdx(neighbourPoint, resolution)];
	}
}

void SimpleBlenderImpl::correctSharedCorner(std::shared_ptr<BlenderChunkData> northEast, std::shared_ptr<BlenderChunkData> southEast, 
    std::shared_ptr<BlenderChunkData> southWest, std::shared_ptr<BlenderChunkData> northWest) {  
    // is the shared corner already finished?
    if (isCornerFinalized(Corner::SouthWest, northEast->flags) &&
        isCornerFinalized(Corner::NorthWest, southEast->flags) &&
        isCornerFinalized(Corner::NorthEast, southWest->flags) &&
        isCornerFinalized(Corner::SouthEast, northWest->flags))
        return;

	int32_t resolution = settings.resolution;
	int32_t radius = blenderSettings.blendingRadius;
	int32_t sampleRadius = blenderSettings.edgeSamplingRadius;

    divideCornerAreaBy2(northEast->mask, Corner::SouthWest, northEast->flags, resolution, radius);
    divideCornerAreaBy2(southEast->mask, Corner::NorthWest, southEast->flags, resolution, radius);
    divideCornerAreaBy2(southWest->mask, Corner::NorthEast, southWest->flags, resolution, radius);
    divideCornerAreaBy2(northWest->mask, Corner::SouthEast, northWest->flags, resolution, radius);

	float average =
		(sumCornerSamples(northEast->chunk, Corner::SouthWest, resolution, sampleRadius) +
		 sumCornerSamples(southEast->chunk, Corner::NorthWest, resolution, sampleRadius) +
		 sumCornerSamples(southWest->chunk, Corner::NorthEast, resolution, sampleRadius) +
		 sumCornerSamples(northWest->chunk, Corner::SouthEast, resolution, sampleRadius))
        / static_cast<float>(4.0f * sampleRadius * sampleRadius);
    
	setCornerCorrection(northEast->chunk, northEast->mask, Corner::SouthWest, average, resolution, radius);
	setCornerCorrection(southEast->chunk, southEast->mask, Corner::NorthWest, average, resolution, radius);
	setCornerCorrection(southWest->chunk, southWest->mask, Corner::NorthEast, average, resolution, radius);
	setCornerCorrection(northWest->chunk, northWest->mask, Corner::SouthEast, average, resolution, radius);

    northEast->flags = markCornerFinalized(Corner::SouthWest, northEast->flags);
    southEast->flags = markCornerFinalized(Corner::NorthWest, southEast->flags);
    southWest->flags = markCornerFinalized(Corner::NorthEast, southWest->flags);
    northWest->flags = markCornerFinalized(Corner::SouthEast, northWest->flags);
}

void SimpleBlenderImpl::applyCorrectionsToChunk(std::shared_ptr<SimpleBlenderImpl::BlenderChunkData> target, float* buffer) {
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

    if (isChunkFullyBlended(target->flags)) {
        applyCorrectionsToChunk(target, buffer);
        return;
    }

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

    // radius x radius corners of each chunk have overlapping influence from 2 adjacent edges
	correctSharedCorner(northeast, east, target, north);
	correctSharedCorner(east, southeast, south, target);
	correctSharedCorner(target, south, southwest, west);
	correctSharedCorner(north, target, west, northwest);

    // after corner correction is applied the edges may no longer match as different chunks will apply different corrections
	correctSharedBoundary(target, north, Edge::North);
	correctSharedBoundary(target, east, Edge::East);
	correctSharedBoundary(target, south, Edge::South);
	correctSharedBoundary(target, west, Edge::West);

    // finalized chunk will be stored in Blender' main cache
	applyCorrectionsToChunk(target, buffer);
}

float SimpleBlenderImpl::getHeight(Vec2Int pos) {
	Vec2Int chunkPos = pointToChunk(pos);
	Vec2Int localPos = pos - chunkPos * std::max(settings.resolution - 1, 1);
	std::vector<float> chunk(settings.resolution * settings.resolution);
	generateChunkData(chunkPos, chunk.data());
	return chunk[localPos.x * settings.resolution + localPos.y];
}

// Blender itself is deterministic, but the baseGenerator might not be
bool SimpleBlenderImpl::isDeterministic() {
    return baseGenerator->isDeterministic();
}

// TODO: manage blender cache
void SimpleBlenderImpl::cleanChunkFromCache(Vec2Int pos) {
	std::lock_guard lock(blenderMutex);
	GeneratorImpl::cleanChunkFromCache(pos);
    baseGenerator->cleanChunkFromCache(pos);
	maskCache.clear();
}

void SimpleBlenderImpl::clearAllCache() {
	std::lock_guard lock(blenderMutex);
	GeneratorImpl::clearAllCache();
    baseGenerator->clearAllCache();
    maskCache.clear();
}
