#include "TestUtils.h"

#include "../library/core/Generators.h"
#include "../library/core/HeightmapGenContext.h"

#include <algorithm>
#include <atomic>
#include <utility>

namespace {

class CountingChunkGeneratorImpl final : public GeneratorImpl {
	std::shared_ptr<std::atomic<int>> generatedChunks;

	float getHeight(Vec2Int) override {
		return 0.0f;
	}

	void generateChunkData(Vec2Int chunkPos, float* buffer) override {
		++*generatedChunks;
		const size_t chunkSize = static_cast<size_t>(settings.resolution) * settings.resolution;
		const float height = static_cast<float>(chunkPos.x * 17 + chunkPos.y * 31);
		std::fill_n(buffer, chunkSize, height);
	}

public:
	CountingChunkGeneratorImpl(
		ContextImpl* ctx,
		const CommonSettings& settings,
		std::shared_ptr<std::atomic<int>> generatedChunks
	)
		: GeneratorImpl(ctx, settings), generatedChunks(std::move(generatedChunks)) {}

	bool isDeterministic() override {
		return true;
	}
};

}

struct CountingChunkGenerator::Impl {
	Generator handle;
	std::shared_ptr<std::atomic<int>> generatedChunks;

	Impl(Context ctx, const CommonSettings& settings)
		: generatedChunks(std::make_shared<std::atomic<int>>(0)) {
		handle = ctx->registerGenerator(
			std::make_shared<CountingChunkGeneratorImpl>(ctx, settings, generatedChunks)
		);
	}
};

CountingChunkGenerator::CountingChunkGenerator(Context ctx, const CommonSettings& settings)
	: impl(std::make_unique<Impl>(ctx, settings)) {}

CountingChunkGenerator::~CountingChunkGenerator() {
	destroy();
}

Generator CountingChunkGenerator::get() const {
	return impl ? impl->handle : nullptr;
}

int CountingChunkGenerator::generatedChunkCount() const {
	return impl ? impl->generatedChunks->load() : 0;
}

void CountingChunkGenerator::destroy() {
	if (impl && impl->handle) {
		destroyGenerator(impl->handle);
		impl->handle = nullptr;
	}
}
