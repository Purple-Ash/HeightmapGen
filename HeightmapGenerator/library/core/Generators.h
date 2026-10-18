#pragma once
#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

struct GeneratorImpl : std::enable_shared_from_this<GeneratorImpl> {
private:
	struct ChunkData {
		std::vector<float> data{};
		bool ready = false;
	};
	std::mutex dataMutex;
	std::condition_variable jobsFinished;
	std::unordered_map<Vec2Int, std::shared_ptr<ChunkData>> cache;
	int32_t jobsInProgress = 0;
	bool drain = false;

	std::shared_ptr<ChunkData> requestChunkImpl(Vec2Int pos, bool forceSingleThreaded);
	void runChunkJob(Vec2Int pos, std::shared_ptr<ChunkData> chunk);
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
	void requestChunk(Vec2Int pos, bool forceSingleThreaded = false);
	void requestPoint(Vec2Int pos, bool forceSingleThreaded = false);
	bool probeChunk(Vec2Int pos, float* buffer);
	bool probePoint(Vec2Int pos, float* point);
	void cleanChunkFromCache(Vec2Int pos);
	void clearAllCache();

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
