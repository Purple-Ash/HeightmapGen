#include "Generators.h"
#include "Helpers.h"
#include <cstdlib>
#include <cmath>

GeneratorImpl::GeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : context(ctx), settings(settings) {}

GeneratorImpl::~GeneratorImpl() {
    ClearAllCache();
}

void GeneratorImpl::generateChunkData(int32_t chunkX, int32_t chunkY, float* buffer) {
    int32_t stride = (settings.resolution > 1) ? (settings.resolution - 1) : 1;
    float originX = static_cast<float>(chunkX * stride);
    float originY = static_cast<float>(chunkY * stride);

    for (int x = 0; x < settings.resolution; x++) {
        for (int y = 0; y < settings.resolution; y++) {
            float posX = originX + static_cast<float>(x);
            float posY = originY + static_cast<float>(y);
            buffer[x * settings.resolution + y] = getHeight(posX, posY);
        }
    }
}

float* GeneratorImpl::GetChunk(int32_t x, int32_t y) {
    Vec2Int pos(x, y);
    if (settings.cacheable) {
        auto it = cache.find(pos);
        if (it != cache.end()) return it->second;
    }

    float* buffer = new float[settings.resolution * settings.resolution];
    generateChunkData(x, y, buffer);

    if (settings.cacheable) {
        cache[pos] = buffer;
    }
    return buffer;
}

float GeneratorImpl::GetPoint(int32_t x, int32_t y) {
    int32_t stride = (settings.resolution > 1) ? (settings.resolution - 1) : 1;

    if (settings.cacheable) {
        int32_t chunkX = (x >= 0) ? (x / stride) : ((x - stride + 1) / stride);
        int32_t chunkY = (y >= 0) ? (y / stride) : ((y - stride + 1) / stride);

        auto it = cache.find(Vec2Int(chunkX, chunkY));
        if (it != cache.end()) {
            int localX = x - (chunkX * stride);
            int localY = y - (chunkY * stride);
            if (localX < settings.resolution && localY < settings.resolution) {
                return it->second[localX * settings.resolution + localY];
            }
        }
    }

    float posX = x * settings.scale;
    float posY = y * settings.scale;
    return getHeight(posX, posY);
}

void GeneratorImpl::RequestChunk(int32_t x, int32_t y) {
    if (!settings.cacheable) return;
    Vec2Int pos(x, y);
    if (cache.find(pos) == cache.end()) {
        float* buffer = new float[settings.resolution * settings.resolution];
        generateChunkData(x, y, buffer);
        cache[pos] = buffer;
    }
}

void GeneratorImpl::RequestPoint(int32_t x, int32_t y) {
    if (!settings.cacheable) return;
    int32_t stride = (settings.resolution > 1) ? (settings.resolution - 1) : 1;
    int32_t chunkX = (x >= 0) ? (x / stride) : ((x - stride + 1) / stride);
    int32_t chunkY = (y >= 0) ? (y / stride) : ((y - stride + 1) / stride);
    RequestChunk(chunkX, chunkY);
}

float* GeneratorImpl::ProbeChunk(int32_t x, int32_t y, bool* ready) {
    if (!settings.cacheable) { *ready = false; return nullptr; }
    auto it = cache.find(Vec2Int(x, y));
    if (it != cache.end()) {
        *ready = true;
        return it->second;
    }
    *ready = false;
    return nullptr;
}

float GeneratorImpl::ProbePoint(int32_t x, int32_t y, bool* ready) {
    if (!settings.cacheable) { *ready = false; return 0.0f; }
    int32_t stride = (settings.resolution > 1) ? (settings.resolution - 1) : 1;
    int32_t chunkX = (x >= 0) ? (x / stride) : ((x - stride + 1) / stride);
    int32_t chunkY = (y >= 0) ? (y / stride) : ((y - stride + 1) / stride);

    auto it = cache.find(Vec2Int(chunkX, chunkY));
    if (it != cache.end()) {
        *ready = true;
        int localX = x - (chunkX * stride);
        int localY = y - (chunkY * stride);
        if (localX < settings.resolution && localY < settings.resolution) {
            return it->second[localX * settings.resolution + localY];
        }
    }
    *ready = false;
    return 0.0f;
}

void GeneratorImpl::CleanChunkFromCache(int32_t x, int32_t y) {
    if (!settings.cacheable) return;
    auto it = cache.find(Vec2Int(x, y));
    if (it != cache.end()) {
        delete[] it->second;
        cache.erase(it);
    }
}

void GeneratorImpl::ClearAllCache() {
    if (!settings.cacheable) return;
    for (auto& pair : cache) {
        delete[] pair.second;
    }
    cache.clear();
}

float RandomGeneratorImpl::getHeight(float posX, float posY) {
    return randomFloatBetween(0.f, 1.f) * settings.amplitude;
}

bool RandomGeneratorImpl::isDeterministic() {
    return false;
}

float CoordinateGeneratorImpl::getHeight(float posX, float posY) {
    return (posX + posY) * settings.amplitude;
}

bool CoordinateGeneratorImpl::isDeterministic() {
    return true;
}