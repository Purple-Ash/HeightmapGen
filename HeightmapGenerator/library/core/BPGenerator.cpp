#include "Generators.h"

#include <numbers>

/// @brief Fills out the given vector with numbers generated similarly to PyTorch.randn()
/// @param buffer Vector to be filled with random numbers
void torchRandn(std::vector<float>& buffer, std::seed_seq& seed) {
	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	for (size_t i = 0; i < buffer.size(); i += 2) {
		float u1 = 1.0f - dist(gen); // avoid log(0)
		float u2 = 1.0f - dist(gen);
		float r = std::sqrt(-2.0f * std::log(u1));
		float theta = 2.0f * std::numbers::pi_v<float> * u2;

		buffer[i] = r * std::cos(theta);
		if (i + 1 < buffer.size()) {
			buffer[i + 1] = r * std::sin(theta);
		}
	}
}

BPGeneratorImpl::BPGeneratorImpl(ContextImpl* ctx, const CommonSettings& commonSettings, const BPGeneratorSettings& bpSettings)
	: OnnxGeneratorImpl(ctx, commonSettings, bpSettings.modelPath) {}

bool BPGeneratorImpl::isDeterministic() {
	return true;
}

void BPGeneratorImpl::fillInput(Vec2Int chunkPos) {
	std::seed_seq seed{
		static_cast<uint32_t>(settings.seed),
		static_cast<uint32_t>(settings.seed >> 32),
		static_cast<uint32_t>(chunkPos.x),
		static_cast<uint32_t>(chunkPos.y)
	};
	torchRandn(inputBuffer, seed);
}
