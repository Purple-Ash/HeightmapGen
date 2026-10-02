#include "Generators.h"
#include "Helpers.h"
#include <cstdint>

static constexpr float whiteNoise(int x, int y, int z) {
	uint32_t seed = uint32_t(x) * 1087;
	seed ^= 0xE56FAA12;
	seed += uint32_t(y) * 2749;
	seed ^= 0x69628a2d;
	seed += uint32_t(z) * 3433;
	seed ^= 0xa7b2c49a;

	return (float) (seed % 1000) / 1000.0f;
}

static constexpr Vec3f sampleVoronoiChunk(int x, int y, int z) {
	return {x + whiteNoise(x, y, z), y + whiteNoise(z + 53, x + 197, y + 967), z + whiteNoise(y + 5, z + 829, x + 541)};
}

static float sampleVoronoi(float x, float y, float z) {
	Vec3f sample {x, y, z};

	int bx = (int) std::floor(x);
	int by = (int) std::floor(y);
	int bz = (int) std::floor(z);

	float value = 1;

	for (int cx = -1; cx <= 1; cx ++) {
		for (int cy = -1; cy <= 1; cy ++) {
			for (int cz = -1; cz <= 1; cz ++) {
				float dist = sampleVoronoiChunk(bx + cx, by + cy, bz + cz).distance(sample);

				if (dist < value) {
					value = dist;
				}
			}
		}
	}

	return value;
}

VoronoiGeneratorImpl::VoronoiGeneratorImpl(ContextImpl* ctx, const CommonSettings& settings)
	: GeneratorImpl(ctx, settings) {
}

float VoronoiGeneratorImpl::getHeight(Vec2Int pos) {
	return sampleVoronoi(pos.x * settings.scale, pos.y * settings.scale, settings.seed) * settings.amplitude;
}

bool VoronoiGeneratorImpl::isDeterministic() {
	return true;
}