#pragma once
#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

struct GeneratorImpl {
protected:
    CommonSettings settings;
    std::unordered_map<Vec2Int, float*> cache;

public:
    ContextImpl* context;

    GeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    virtual ~GeneratorImpl();

    virtual float getHeight(float posX, float posY) = 0;
    virtual void generateChunkData(int32_t chunkX, int32_t chunkY, float* buffer);

    float* GetChunk(int32_t x, int32_t y);
    float GetPoint(int32_t x, int32_t y);
    void RequestChunk(int32_t x, int32_t y);
    void RequestPoint(int32_t x, int32_t y);
    float* ProbeChunk(int32_t x, int32_t y, bool* ready);
    float ProbePoint(int32_t x, int32_t y, bool* ready);
    void CleanChunkFromCache(int32_t x, int32_t y);
    void ClearAllCache();
    
    virtual bool isDeterministic() = 0;
};

class CoordinateGeneratorImpl : public GeneratorImpl {
public:
    CoordinateGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : GeneratorImpl(ctx, settings) {}
    float getHeight(float posX, float posY) override;
    bool isDeterministic() override;
};

class RandomGeneratorImpl : public GeneratorImpl {
public:
    RandomGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : GeneratorImpl(ctx, settings) {}
    float getHeight(float posX, float posY) override;
    bool isDeterministic() override;
};

class PerlinGeneratorImpl : public GeneratorImpl {
public:
    PerlinGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    float getHeight(float posX, float posY) override;
    bool isDeterministic() override;
};

class BrownianPerlinGeneratorImpl : public GeneratorImpl {
    std::vector<PerlinGeneratorImpl> octaves;
public:
    BrownianPerlinGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    float getHeight(float posX, float posY) override;
    bool isDeterministic() override;
};

class HydraulicErosionGeneratorImpl : public GeneratorImpl {
    HydraulicErosionSettings hydraulicErosionSettings;
public:
    HydraulicErosionGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings, const HydraulicErosionSettings& erosionSettings);
    float getHeight(float posX, float posY) override;
    void generateChunkData(int32_t chunkX, int32_t chunkY, float* buffer) override;
    bool isDeterministic() override;
};