#pragma once
#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include "onnxruntime_cxx_api.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>

struct GeneratorImpl : std::enable_shared_from_this<GeneratorImpl> {
private:
	struct ChunkData {
		std::vector<float> data{};
		bool ready = false;
		bool started = false;
	};
	std::mutex dataMutex;
	std::condition_variable jobsFinished;
	std::unordered_map<Vec2Int, std::shared_ptr<ChunkData>> cache;
	int32_t jobsInProgress = 0;
	std::atomic<bool> drain{false};

	std::shared_ptr<ChunkData> requestChunkImpl(Vec2Int pos, bool forceSingleThreaded);
	void runChunkJob(Vec2Int pos, std::shared_ptr<ChunkData> chunk, bool alreadyStarted);
	std::shared_ptr<ChunkData> getChunkData(Vec2Int pos, bool forceSingleThreaded);
	float pointFromChunk(Vec2Int pos, const std::vector<float>& data) const;

protected:
	ContextImpl* context;
	CommonSettings settings;

	virtual float getHeight(Vec2Int pos) = 0;
	virtual void generateChunkData(Vec2Int chunkPos, float* buffer);

public:
	GeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
	virtual ~GeneratorImpl();

	void getChunk(Vec2Int pos, float* buffer, bool forceSingleThreaded = false);
	float getPoint(Vec2Int pos, bool forceSingleThreaded = false);
	virtual void requestChunk(Vec2Int pos, bool forceSingleThreaded = false);
	void requestPoint(Vec2Int pos, bool forceSingleThreaded = false);
	bool probeChunk(Vec2Int pos, float* buffer);
	bool probePoint(Vec2Int pos, float* point);
	virtual void cleanChunkFromCache(Vec2Int pos);
	virtual void clearAllCache();

	virtual bool isDeterministic() = 0;

	ContextImpl* getContext() const;
	const CommonSettings& getSettings() const;

	Vec2Int pointToChunk(Vec2Int point) const;

	void waitForAll();
	void fastDrain();
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
	friend class BrownianPerlinGeneratorImpl;
	float getHeight(Vec2Int pos) override;

public:
	PerlinGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings);
	bool isDeterministic() override;
};

class BrownianPerlinGeneratorImpl : public GeneratorImpl {
	std::vector<std::unique_ptr<PerlinGeneratorImpl>> octaves;
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
	std::shared_ptr<GeneratorImpl> baseGenerator;

	float getHeight(Vec2Int pos) override;
	void generateChunkData(Vec2Int chunkPos, float* buffer) override;

public:
    HydraulicErosionGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings, const HydraulicErosionSettings& erosionSettings);
    bool isDeterministic() override;
};

class OnnxGeneratorImpl : public GeneratorImpl {
    std::mutex inferenceMutex;
    Ort::Session session;
    Ort::MemoryInfo memoryInfo;
    Ort::IoBinding ioBinding;
    Ort::AllocatorWithDefaultOptions allocator;
    Ort::RunOptions runOptions;

    Ort::AllocatedStringPtr inputLayerName;
protected:
    std::vector<int64_t> inputTensorShape;
    std::size_t inputTensorElements;

    std::vector<float> inputBuffer;
    Ort::Value inputValue;

    Ort::AllocatedStringPtr outputLayerName;
    std::vector<int64_t> outputTensorShape;
    std::size_t outputTensorElements;

    virtual void fillInput(Vec2Int chunkPos) = 0;
    void generateChunkData(Vec2Int chunkPos, float* buffer) override;
    float getHeight(Vec2Int pos) override;

public:
    OnnxGeneratorImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const char* modelPath);
};

/// @brief Neural-network powered terrain generator, adapted from "A step towards procedural terrain generation with GANs" 2017 Beckham, Pal
///
/// Paper: https://arxiv.org/abs/1707.03383
/// Github: https://github.com/christopher-beckham/gan-heightmaps
///
class BPGeneratorImpl : public OnnxGeneratorImpl {
    void fillInput(Vec2Int chunkPos) override;

public:
    BPGeneratorImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const BPGeneratorSettings& bpSettings);
    bool isDeterministic() override;
};

class ModelErosionGeneratorImpl : public OnnxGeneratorImpl {
    std::shared_ptr<GeneratorImpl> baseGenerator;
    void fillInput(Vec2Int chunkPos) override;

public:
    ModelErosionGeneratorImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const ModelErosionSettings& modelErosionSettings);

    bool isDeterministic() override;
};

class SimpleBlenderImpl : public GeneratorImpl {
private:
	struct BlenderChunkData {
		uint8_t flags = 0;
		std::vector<float> mask{};
		std::vector<float> chunk{};
	};
	BlenderSettings blenderSettings;
	std::shared_ptr<GeneratorImpl> baseGenerator;
	std::mutex blenderMutex;
    std::unordered_map<Vec2Int, std::shared_ptr<BlenderChunkData>> maskCache;

    std::shared_ptr<BlenderChunkData> getBlenderChunkData(Vec2Int pos);
	void calculateCorrectionAlongEdge(std::shared_ptr<BlenderChunkData> target, std::shared_ptr<BlenderChunkData> neighbour, Edge edge);
	void correctSharedCorner(std::shared_ptr<BlenderChunkData> northEast, std::shared_ptr<BlenderChunkData> southEast, std::shared_ptr<BlenderChunkData> southWest, std::shared_ptr<BlenderChunkData> northWest);
    void correctSharedBoundary(std::shared_ptr<BlenderChunkData> first, std::shared_ptr<BlenderChunkData> second, Edge edge);
    void applyCorrectionsToChunk(std::shared_ptr<SimpleBlenderImpl::BlenderChunkData> chunkAndMask, float* buffer);

protected:
	float getHeight(Vec2Int pos) override;
	void generateChunkData(Vec2Int chunkPos, float* buffer) override;

public:
	SimpleBlenderImpl(ContextImpl* ctx, const CommonSettings& settings, const BlenderSettings& blenderSettings);
	void requestChunk(Vec2Int pos, bool forceSingleThreaded = false) override;
    	
	bool isDeterministic() override;
	void cleanChunkFromCache(Vec2Int pos) override;
	void clearAllCache() override;
};
