#include "Generators.h"
#include "Helpers.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <span>

GeneratorImpl::GeneratorImpl(ContextImpl* ctx, const CommonSettings& settings) : context(ctx), settings(settings) {}

GeneratorImpl::~GeneratorImpl() = default;

void GeneratorImpl::generateChunkData(Vec2Int chunkPos, float* buffer) {
	int32_t stride = std::max(settings.resolution - 1, 1);
	Vec2Int origin = chunkPos * stride;
	for (int32_t x = 0; x < settings.resolution; x++) {
		for (int32_t y = 0; y < settings.resolution; y++) {
			buffer[x * settings.resolution + y] = getHeight(origin + Vec2Int{x, y});
		}
	}
}

void GeneratorImpl::waitForAll() {
	std::unique_lock lock(dataMutex);
	jobsFinished.wait(lock, [this] {
		return jobsInProgress == 0;
	});
}

std::shared_ptr<GeneratorImpl::ChunkData> GeneratorImpl::getChunkData(Vec2Int pos, bool forceSingleThreaded) {
	auto chunk = requestChunkImpl(pos, forceSingleThreaded);

	std::unique_lock lock(dataMutex);
	jobsFinished.wait(lock, [&chunk] {
		return chunk->ready;
	});

	return chunk;
}

void GeneratorImpl::getChunk(Vec2Int pos, float* buffer, bool forceSingleThreaded) {
	if (!buffer) { 
		return;
	}
	if (settings.cacheable) {
		auto data = getChunkData(pos, forceSingleThreaded);
		std::copy(data->data.begin(), data->data.end(), buffer);
		return;
	}
	generateChunkData(pos, buffer);
}

float GeneratorImpl::getPoint(Vec2Int pos, bool forceSingleThreaded) {
	if (settings.cacheable) {
		auto data = getChunkData(pointToChunk(pos), forceSingleThreaded);
		return pointFromChunk(pos, data->data);
	}
	return getHeight(pos);
}

void GeneratorImpl::runChunkJob(Vec2Int pos, std::shared_ptr<ChunkData> chunk) {
	std::vector<float> generated(settings.resolution * settings.resolution);
	generateChunkData(pos, generated.data());
	{
		std::lock_guard lock(dataMutex);
		chunk->data = std::move(generated);
		chunk->ready = true;
		jobsInProgress--;
	}
	jobsFinished.notify_all();
}

std::shared_ptr<GeneratorImpl::ChunkData> GeneratorImpl::requestChunkImpl(Vec2Int pos, bool forceSingleThreaded) {
	if (!settings.cacheable) return nullptr;

	std::shared_ptr<ChunkData> chunk;
	{
		std::lock_guard lock(dataMutex);
		auto it = cache.find(pos);
		// If the chunk is already in the cache but is not ready and we are forcing single-threaded execution,
		// we overwrite the existing chunk with a new one to ensure that the chunk is generated in the current thread.
		// This prevents potential deadlocks when the pool is full with HEG jobs.
		// TODO: Cancel the existing job instead of overwriting it, if it has not started yet.
		if (it != cache.end() && (it->second->ready || !forceSingleThreaded)) {
			return it->second;
		}
		chunk = std::make_shared<ChunkData>();
		cache[pos] = chunk;
		jobsInProgress++;
	}

	if (context->numThreads() > 0 && !forceSingleThreaded) {
		auto self = shared_from_this();
		context->enqueue([self, pos, chunk] {
			self->runChunkJob(pos, chunk);
		});
	}
	else {
		runChunkJob(pos, chunk);
	}
	return chunk;
}

void GeneratorImpl::requestChunk(Vec2Int pos, bool forceSingleThreaded) {
	requestChunkImpl(pos, forceSingleThreaded);
}

void GeneratorImpl::requestPoint(Vec2Int pos, bool forceSingleThreaded) {
	if (!settings.cacheable) return;
	requestChunk(pointToChunk(pos), forceSingleThreaded);
}

bool GeneratorImpl::probeChunk(Vec2Int pos, float* buffer) {
	if (!settings.cacheable) return false;

	std::shared_ptr<ChunkData> data;
	{
		std::lock_guard lock(dataMutex);
		auto it = cache.find(pos);
		if (it == cache.end() || !it->second->ready) {
			return false;
		}
		data = it->second;
	}
	if (buffer) {
		std::copy(data->data.begin(), data->data.end(), buffer);
	}
	return true;
}

bool GeneratorImpl::probePoint(Vec2Int pos, float* point) {
	if (!settings.cacheable) return false;

	std::shared_ptr<ChunkData> data;
	{
		std::lock_guard lock(dataMutex);
		auto it = cache.find(pointToChunk(pos));
		if (it == cache.end() || !it->second->ready) {
			return false;
		}
		data = it->second;
	}
	if (point) {
		*point = pointFromChunk(pos, data->data);
	}
	return true;
}

void GeneratorImpl::cleanChunkFromCache(Vec2Int pos) {
	std::lock_guard lock(dataMutex);
	cache.erase(pos);
}

void GeneratorImpl::clearAllCache() {
	std::lock_guard lock(dataMutex);
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

float GeneratorImpl::pointFromChunk(Vec2Int pos, const std::vector<float>& data) const {
	const auto stride = std::max(settings.resolution - 1, 1);
	const auto local = pos - pointToChunk(pos) * stride;
	return data[local.x * settings.resolution + local.y];
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
