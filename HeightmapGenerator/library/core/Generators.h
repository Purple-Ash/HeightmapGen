#pragma once
#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include "onnxruntime_cxx_api.h"
#include <vector>
#include <unordered_map>
#include <system_error>

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
	float amplitude;

    const float lacunarity = 2.0f;
    const float persistence = 0.5f;
    const unsigned int octaveCount = 4;

    float getHeight(Vec2Int pos) override;

public:
    BrownianPerlinGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
    bool isDeterministic() override;
};

class VoronoiGeneratorImpl : public GeneratorImpl {
	float getHeight(Vec2Int pos) override;

	public:
		VoronoiGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
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

// TODO: make thread-safe version of generator
// TODO: make GPU-capable version of generator
/// @brief Neural-network powered terrain generator, adapted from "A step towards procedural terrain generation with GANs" 2017 Beckham, Pal
/// 
/// Paper: https://arxiv.org/abs/1707.03383
/// Github: https://github.com/christopher-beckham/gan-heightmaps
///
/// Currently only single-threaded and CPU-based
class BPGeneratorImpl : public GeneratorImpl {
    /// @brief Model session, MUST not live longer then Ort::Env used to create it 
    Ort::Session session;

    // TODO: extend for different memory types
    /// @brief Descriptor of memory used by the model, currently defaults to CPU
    Ort::MemoryInfo memoryInfo;

    /// @brief Use to arrange the input/output memory used by the model
    Ort::IoBinding ioBinding;

    // TODO: use factory instead of referencing allocator directly
    /// @brief Ort-owned default allocator, used for allocating input/output names
    Ort::AllocatorWithDefaultOptions allocator;
    
    // TODO: allow changing them
    /// @brief per-inference run options
    Ort::RunOptions runOptions;

    Ort::AllocatedStringPtr inputLayerName;
    std::vector<int64_t> inputTensorShape;
    std::size_t inputTensorElements;

    // WARN: reusing the buffer saves memory, but makes the generator not thread-safe
    /// @brief reusable buffer for random input tensor
    std::vector<float> inputBuffer;
    /// @brief reusable tensor wrapper around inputBuffer
    Ort::Value inputValue;
    
    Ort::AllocatedStringPtr outputLayerName;
    std::vector<int64_t> outputTensorShape;
    // effectively resolution^2 and number of elements in output buffer
    std::size_t outputTensorElements; 

    void generateChunkData(Vec2Int chunkPos, float* buffer) override;
    float getHeight(Vec2Int pos) override;

public:
    /// @brief 
    /// @param ctx base generator context
    /// @param commonSettings mostly unused, save for cacheability. Resolution will be overriden by the model's real resolution
    /// @param modelPath relative path to .onnx file with exported model
    /// @param ec error code return
    BPGeneratorImpl(
        ContextImpl* ctx, 
        const CommonSettings& commonSettings, 
        const char* modelPath,
        std::error_code& ec
    );

    bool isDeterministic() override;
};
