#include "Generators.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace {
const Ort::Env& runtimeEnvironment() {
	static const Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "HeightmapGen");
	return env;
}

Ort::SessionOptions makeSessionOptions() {
	Ort::SessionOptions options;
	options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
	return options;
}
}

OnnxGeneratorImpl::OnnxGeneratorImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const char* modelPath)
	: GeneratorImpl(ctx, commonSettings),
	  session(runtimeEnvironment(), std::filesystem::u8path(modelPath).c_str(), makeSessionOptions()),
	  memoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
	  ioBinding(session),
	  inputLayerName(session.GetInputNameAllocated(0, allocator)),
	  inputTensorShape(session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape()),
	  inputTensorElements(session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetElementCount()),
	  inputBuffer(inputTensorElements),
	  inputValue(Ort::Value::CreateTensor<float>(
		  memoryInfo,
		  inputBuffer.data(),
		  inputBuffer.size(),
		  inputTensorShape.data(),
		  inputTensorShape.size())
	  ),
	  outputLayerName(session.GetOutputNameAllocated(0, allocator)),
	  outputTensorShape(session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape()),
	  outputTensorElements(session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetElementCount()) {
	if (outputTensorShape[2] != outputTensorShape[3] || outputTensorShape[2] != commonSettings.resolution) {
		throw std::invalid_argument("Model output resolution does not match the requested resolution");
	}

	ioBinding.BindInput(inputLayerName.get(), inputValue);
}

void OnnxGeneratorImpl::generateChunkData(Vec2Int chunkPos, float* buffer) {
	std::lock_guard lock(inferenceMutex);
	fillInput(chunkPos);

	auto outputValue = Ort::Value::CreateTensor<float>(
		memoryInfo,
		buffer,
		outputTensorElements,
		outputTensorShape.data(),
		outputTensorShape.size()
	);

	ioBinding.ClearBoundOutputs();
	ioBinding.BindOutput(outputLayerName.get(), outputValue);

	session.Run(runOptions, ioBinding);

	for (size_t i = 0; i < outputTensorElements; i++) {
		buffer[i] *= settings.amplitude;
	}
}

float OnnxGeneratorImpl::getHeight(Vec2Int pos) {
	const int32_t stride = std::max(settings.resolution - 1, 1);
	const Vec2Int chunkPos = pointToChunk(pos);
	// getChunk will create a new cache entry. Later getPoint calls will be able to reuse the cached data and getHeight will not be called again.
	std::vector<float> chunkData(settings.resolution * settings.resolution);
	getChunk(chunkPos, chunkData.data());
	const Vec2Int localPos = pos - chunkPos * stride;
	if (localPos.x < 0 || localPos.x >= settings.resolution || localPos.y < 0 || localPos.y >= settings.resolution) {
		return 0.0f;
	}
	return chunkData[localPos.x * settings.resolution + localPos.y];
}
