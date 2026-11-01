#include "Generators.h"

#include <algorithm>
#include <stdexcept>

ModelErosionGeneratorImpl::ModelErosionGeneratorImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const ModelErosionSettings& modelErosionSettings)
	: OnnxGeneratorImpl(ctx, commonSettings, modelErosionSettings.modelPath), baseGenerator(ctx->getGenerator(modelErosionSettings.baseGeneratorImpl)) {
	if (!baseGenerator) {
		throw std::invalid_argument("Model erosion generator requires a base generator");
	}
}

bool ModelErosionGeneratorImpl::isDeterministic() {
	return baseGenerator->isDeterministic();
}

void ModelErosionGeneratorImpl::fillInput(Vec2Int chunkPos) {
	const int32_t inputSize = inputTensorShape[2];
	const int32_t outputSize = settings.resolution;
	const int32_t stride = std::max(outputSize - 1, 1);
	const Vec2Int outputOrigin = chunkPos * stride;
	const int32_t contextOffset = (inputSize - outputSize) / 2;
	const Vec2Int inputOrigin = outputOrigin - Vec2Int{ contextOffset, contextOffset };

	for (int32_t x = 0; x < inputSize; x++) {
		for (int32_t y = 0; y < inputSize; y++) {
			inputBuffer[x * inputSize + y] = baseGenerator->getPoint(inputOrigin + Vec2Int{ x, y }, true);
		}
	}
}
