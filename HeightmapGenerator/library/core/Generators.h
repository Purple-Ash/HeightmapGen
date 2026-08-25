#pragma once
#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <span>

struct GeneratorImpl {
protected:
    ContextImpl* context;
    CommonSettings settings;
    std::unordered_map<Vec2Int, std::vector<float>> cache;

    virtual float getHeight(Vec2Int pos) = 0;
    virtual void generateChunkData(Vec2Int chunkPos, float* buffer);

public:
    GeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    virtual ~GeneratorImpl();

    void getChunk(Vec2Int pos, float* buffer);
    float getPoint(Vec2Int pos);
    void requestChunk(Vec2Int pos);
    void requestPoint(Vec2Int pos);
    bool probeChunk(Vec2Int pos, float* buffer);
    bool probePoint(Vec2Int pos, float* point);
    void cleanChunkFromCache(Vec2Int pos);
    void clearAllCache();

    virtual bool isDeterministic() = 0;

    ContextImpl* getContext() const;
    const CommonSettings& getSettings() const;

    Vec2Int pointToChunk(Vec2Int point) const;
};

class CoordinateGeneratorImpl : public GeneratorImpl {
    float getHeight(Vec2Int pos) override;

public:
    CoordinateGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : GeneratorImpl(ctx, settings) {}
    bool isDeterministic() override;
};

class RandomGeneratorImpl : public GeneratorImpl {
    float getHeight(Vec2Int pos) override;

public:
    RandomGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : GeneratorImpl(ctx, settings) {}
    bool isDeterministic() override;
};

class PerlinGeneratorImpl : public GeneratorImpl {
    float getHeight(Vec2Int pos) override;

public:
    PerlinGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    bool isDeterministic() override;
};

class BrownianPerlinGeneratorImpl : public GeneratorImpl {
    std::vector<PerlinGeneratorImpl> octaves;

    float getHeight(Vec2Int pos) override;

public:
    BrownianPerlinGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    bool isDeterministic() override;
};

class HydraulicErosionGeneratorImpl : public GeneratorImpl {
    HydraulicErosionSettings hydraulicErosionSettings;

    float getHeight(Vec2Int pos) override;
    void generateChunkData(Vec2Int chunkPos, float* buffer) override;

public:
    HydraulicErosionGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings, const HydraulicErosionSettings& erosionSettings);
    bool isDeterministic() override;
};