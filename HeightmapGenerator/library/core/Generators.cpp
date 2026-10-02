#include "Generators.h"
#include "Helpers.h"
#include <cstdlib>
#include <cmath>

GeneratorImpl::GeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : context(ctx), settings(settings) {}

GeneratorImpl::~GeneratorImpl() {
    clearAllCache();
}

void GeneratorImpl::generateChunkData(Vec2Int chunkPos, float* buffer) {
	int32_t stride = std::max(settings.resolution - 1, 1);
	Vec2Int origin = chunkPos * stride;

    Vec2Int pos;
    for (pos.x = 0; pos.x < settings.resolution; pos.x++) {
        for (pos.y = 0; pos.y < settings.resolution; pos.y++) {
			buffer[pos.x * settings.resolution + pos.y] = getHeight(origin + pos);
        }
    }
}

void GeneratorImpl::getChunk(Vec2Int pos, float* buffer) {
    if (settings.cacheable) {
        auto it = cache.find(pos);
        if (it != cache.end()) {
			std::copy(it->second.begin(), it->second.end(), buffer);
            return;
        }

        std::vector<float> newData(settings.resolution * settings.resolution);
        generateChunkData(pos, newData.data());
        std::copy(newData.begin(), newData.end(), buffer);
        cache[pos] = std::move(newData);
    }
    else {
		generateChunkData(pos, buffer);
    }
}

float GeneratorImpl::getPoint(Vec2Int pos) {
    if (settings.cacheable) {
		Vec2Int chunkPos = pointToChunk(pos);

        auto it = cache.find(chunkPos);
        if (it != cache.end()) {
            int32_t stride = std::max(settings.resolution - 1, 1);
			Vec2Int localPos = pos - (chunkPos * stride);
            if (localPos.x < settings.resolution && localPos.y < settings.resolution) {
                return it->second[localPos.x * settings.resolution + localPos.y];
            }
        }
    }

    return getHeight(pos);
}

void GeneratorImpl::requestChunk(Vec2Int pos) {
    if (!settings.cacheable) return;
    if (cache.find(pos) == cache.end()) {
        std::vector<float> newData(settings.resolution * settings.resolution);
        generateChunkData(pos, newData.data());
        cache[pos] = std::move(newData);
    }
}

void GeneratorImpl::requestPoint(Vec2Int pos) {
    if (!settings.cacheable) return;
    requestChunk(pointToChunk(pos));
}

bool GeneratorImpl::probeChunk(Vec2Int pos, float* buffer) {
    if (!settings.cacheable) { return false; }
    auto it = cache.find(pos);
    if (it != cache.end()) {
        if (buffer) {
            std::copy(it->second.begin(), it->second.end(), buffer);
		}
        return true;
    }
    return false;
}

bool GeneratorImpl::probePoint(Vec2Int pos, float* point) {
    if (!settings.cacheable) { return false; }
    int32_t stride = std::max(settings.resolution - 1, 1);
	Vec2Int chunkPos = pointToChunk(pos);

    auto it = cache.find(chunkPos);
    if (it != cache.end()) {
        Vec2Int localPos = pos - (chunkPos * stride);
        if (localPos.x < settings.resolution && localPos.y < settings.resolution) {
            if (point) {
                *point = it->second[localPos.x * settings.resolution + localPos.y];
            }
            return true;
        }
    }

	return false;
}

void GeneratorImpl::cleanChunkFromCache(Vec2Int chunkPos) {
    if (!settings.cacheable) return;
    auto it = cache.find(chunkPos);
    if (it != cache.end()) {
        cache.erase(it);
    }
}

void GeneratorImpl::clearAllCache() {
    if (!settings.cacheable) return;
    cache.clear();
}

ContextImpl* GeneratorImpl::getContext() const {
    return context;
}

const CommonSettings& GeneratorImpl::getSettings() const {
    return settings;
}

Vec2Int GeneratorImpl::pointToChunk(Vec2Int point) const {
    int32_t stride = std::max(settings.resolution - 1, 1);
    return Vec2Int(
        ((point.x >= 0) ? point.x : (point.x - stride + 1)) / stride,
        ((point.y >= 0) ? point.y : (point.y - stride + 1)) / stride
    );
}

float RandomGeneratorImpl::getHeight(Vec2Int) {
    return randomFloatBetween(0.f, 1.f) * settings.amplitude;
}

bool RandomGeneratorImpl::isDeterministic() {
    return false;
}

float CoordinateGeneratorImpl::getHeight(Vec2Int pos) {
    return (pos.x * settings.scale + pos.y * settings.scale) * settings.amplitude;
}

bool CoordinateGeneratorImpl::isDeterministic() {
    return true;
}