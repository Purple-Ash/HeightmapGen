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
    // Non-owning: the base generator is created and destroyed independently by the caller
    // via the C API (e.g. CreatePerlinGenerator), and just referenced here by handle.
    GeneratorImpl* baseGenerator = nullptr;
public:
    HydraulicErosionGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings, const HydraulicErosionSettings& erosionSettings);
    float getHeight(float posX, float posY) override;
    void generateChunkData(int32_t chunkX, int32_t chunkY, float* buffer) override;
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
public:
    /// @brief 
    /// @param ctx base generator context
    /// @param commonSettings mostly unused, save for cacheability. Resolution will be overriden by the model's real resolution
    /// @param env Ort::Env used to create session, there should only ever be one per process. BPGenerator MUST not live longer then the env.
    /// @param sessionOptions options for model session
    /// @param modelPath relative path to .onnx file with exported model
    BPGeneratorImpl(
        ContextImpl* ctx, 
        const CommonSettings& commonSettings, 
        const Ort::Env& env,
        const Ort::SessionOptions& sessionOptions,
        const char* modelPath
    );
    void generateChunkData(int32_t chunkX, int32_t chunkY, float* buffer) override;
    bool isDeterministic() override;
    float getHeight(float posX, float posY) override;
    /// @brief Return the resolution read from model
    int64_t getResolution();
    size_t getOutputElementsCount();
};