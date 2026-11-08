#include "Blender.h"

#include <algorithm>
#include <stdexcept>

namespace {

size_t maskIndex(int32_t x, int32_t y, int32_t resolution, int32_t radius) {
	if (y < radius && x < resolution - radius && x >= radius) {
		return static_cast<size_t>(radius) * resolution +
			static_cast<size_t>(y) * (resolution - 2 * radius) + (x - radius);
	}

	if (y >= resolution - radius && x < resolution - radius && x >= radius) {
		return static_cast<size_t>(2 * radius) * (resolution - radius) +
			static_cast<size_t>(y + radius - resolution) * (resolution - 2 * radius) + (x - radius);
	}

	if (x < radius) {
		return static_cast<size_t>(x) * resolution + y;
	}

	if (x >= resolution - radius) {
		return static_cast<size_t>(radius) * (3 * resolution - 4 * radius) +
			static_cast<size_t>(x - resolution + radius) * resolution + y;
	}

	throw std::out_of_range("Coordinate is outside the blending mask");
}

float smoothstep(float t) {
	return t * t * (3.0f - 2.0f * t);
}

}

BlenderImpl::BlenderImpl(
	std::shared_ptr<GeneratorImpl> source,
	int32_t radius,
	int32_t edgeSamplingRadius
)
	: GeneratorImpl(
		source ? source->getContext() : nullptr,
		source ? source->getSettings() : CommonSettings{}
	),
	  source(std::move(source)),
	  radius(radius),
	  edgeSamplingRadius(edgeSamplingRadius) {
	if (!this->source) {
		throw std::invalid_argument("Blender requires a source generator");
	}
	if (settings.resolution < 2 || radius <= 0 ||
		static_cast<int64_t>(radius) * 2 > settings.resolution) {
		throw std::invalid_argument("Blender radius must be positive and no greater than half the resolution");
	}
	if (edgeSamplingRadius <= 0 || edgeSamplingRadius > settings.resolution) {
		throw std::invalid_argument("Blender edge-sampling radius must be positive and no greater than the resolution");
	}
}

BlenderImpl::ChunkData& BlenderImpl::getChunkData(Vec2Int pos) {
	auto [it, inserted] = maskCache.try_emplace(pos);
	if (inserted) {
		const size_t chunkSize = static_cast<size_t>(settings.resolution) * settings.resolution;
		it->second.heights.resize(chunkSize);
		it->second.mask.resize(
			static_cast<size_t>(4) * radius * (settings.resolution - radius),
			0.0f
		);
		source->getChunk(pos, it->second.heights.data(), true);
	}
	return it->second;
}

float BlenderImpl::averageEdgeSamples(
	const ChunkData& target,
	const ChunkData& neighbour,
	Edge edge,
	int32_t alongEdge
) const {
	const int32_t resolution = settings.resolution;
	float total = 0.0f;
	for (int32_t depth = 0; depth < edgeSamplingRadius; ++depth) {
		int32_t targetX;
		int32_t targetY;
		int32_t neighbourX;
		int32_t neighbourY;

		switch (edge) {
		case Edge::North:
			targetX = alongEdge;
			targetY = depth;
			neighbourX = alongEdge;
			neighbourY = resolution - depth - 1;
			break;
		case Edge::East:
			targetX = resolution - depth - 1;
			targetY = alongEdge;
			neighbourX = depth;
			neighbourY = alongEdge;
			break;
		case Edge::South:
			targetX = alongEdge;
			targetY = resolution - depth - 1;
			neighbourX = alongEdge;
			neighbourY = depth;
			break;
		case Edge::West:
			targetX = depth;
			targetY = alongEdge;
			neighbourX = resolution - depth - 1;
			neighbourY = alongEdge;
			break;
		}

		total += target.heights[targetX * resolution + targetY];
		total += neighbour.heights[neighbourX * resolution + neighbourY];
	}
	return total / static_cast<float>(2 * edgeSamplingRadius);
}

void BlenderImpl::smoothEdge(ChunkData& target, ChunkData& neighbour, Edge edge) {
	const int32_t resolution = settings.resolution;
	for (int32_t i = 0; i < resolution; ++i) {
		for (int32_t j = 0; j < radius; ++j) {
			int32_t targetX;
			int32_t targetY;
			int32_t neighbourX;
			int32_t neighbourY;

			switch (edge) {
			case Edge::North:
				targetX = i;
				targetY = j;
				neighbourX = i;
				neighbourY = resolution - j - 1;
				break;
			case Edge::East:
				targetX = resolution - j - 1;
				targetY = i;
				neighbourX = j;
				neighbourY = i;
				break;
			case Edge::South:
				targetX = i;
				targetY = resolution - j - 1;
				neighbourX = i;
				neighbourY = j;
				break;
			case Edge::West:
				targetX = j;
				targetY = i;
				neighbourX = resolution - j - 1;
				neighbourY = i;
				break;
			}

			const float a = target.heights[targetX * resolution + targetY];
			const float b = neighbour.heights[neighbourX * resolution + neighbourY];
			const float weight = smoothstep(1.0f - static_cast<float>(j) / radius);
			const float average = j == 0
				? averageEdgeSamples(target, neighbour, edge, i)
				: (a + b) / 2.0f;
			target.mask[maskIndex(targetX, targetY, resolution, radius)] += weight * (average - a);
			neighbour.mask[maskIndex(neighbourX, neighbourY, resolution, radius)] += weight * (average - b);
		}
	}
}

void BlenderImpl::correctCorner(
	const std::array<CornerSample, 4>& samples,
	Vec2Int vertex
) {
	if (!processedCorners.insert(vertex).second) {
		return;
	}

	const int32_t resolution = settings.resolution;
	for (const auto& sample : samples) {
		for (int32_t x = 0; x < radius; ++x) {
			for (int32_t y = 0; y < radius; ++y) {
				const auto index = maskIndex(x + sample.regionX, y + sample.regionY, resolution, radius);
				sample.chunk->mask[index] /= 2.0f;
			}
		}
	}

	float average = 0.0f;
	for (const auto& sample : samples) {
		const int32_t firstX = sample.x == 0 ? 0 : resolution - edgeSamplingRadius;
		const int32_t firstY = sample.y == 0 ? 0 : resolution - edgeSamplingRadius;
		for (int32_t x = 0; x < edgeSamplingRadius; ++x) {
			for (int32_t y = 0; y < edgeSamplingRadius; ++y) {
				average += sample.chunk->heights[
					(firstX + x) * resolution + firstY + y
				];
			}
		}
	}
	average /= static_cast<float>(samples.size() * edgeSamplingRadius * edgeSamplingRadius);

	for (const auto& sample : samples) {
		sample.chunk->mask[maskIndex(sample.x, sample.y, resolution, radius)] =
			average - sample.chunk->heights[sample.x * resolution + sample.y];
	}
}

void BlenderImpl::correctSharedBoundaries() {
	const int32_t resolution = settings.resolution;
	const int32_t stride = std::max(resolution - 1, 1);
	for (auto& [pos, data] : maskCache) {
		auto correct = [this, resolution, stride](Vec2Int firstPos, ChunkData& first, ChunkData& second, Edge edge) {
			for (int32_t i = 0; i < resolution; ++i) {
				int32_t firstX;
				int32_t firstY;
				int32_t secondX;
				int32_t secondY;
				Vec2Int vertex;
				switch (edge) {
				case Edge::North:
					firstX = i; firstY = 0;
					secondX = i; secondY = resolution - 1;
					vertex = {firstPos.x * stride + (i == 0 ? 0 : stride), firstPos.y * stride};
					break;
				case Edge::East:
					firstX = resolution - 1; firstY = i;
					secondX = 0; secondY = i;
					vertex = {firstPos.x * stride + stride, firstPos.y * stride + (i == 0 ? 0 : stride)};
					break;
				case Edge::South:
					firstX = i; firstY = resolution - 1;
					secondX = i; secondY = 0;
					vertex = {firstPos.x * stride + (i == 0 ? 0 : stride), firstPos.y * stride + stride};
					break;
				case Edge::West:
					firstX = 0; firstY = i;
					secondX = resolution - 1; secondY = i;
					vertex = {firstPos.x * stride, firstPos.y * stride + (i == 0 ? 0 : stride)};
					break;
				}
				if ((i == 0 || i == resolution - 1) && processedCorners.contains(vertex)) {
					continue;
				}
				const float a = first.heights[firstX * resolution + firstY];
				const float b = second.heights[secondX * resolution + secondY];
				const float average = averageEdgeSamples(first, second, edge, i);
				first.mask[maskIndex(firstX, firstY, resolution, radius)] = average - a;
				second.mask[maskIndex(secondX, secondY, resolution, radius)] = average - b;
			}
		};

		if (data.eastEdgeProcessed) {
			auto neighbour = maskCache.find(pos + Vec2Int{1, 0});
			if (neighbour != maskCache.end()) {
				correct(pos, data, neighbour->second, Edge::East);
			}
		}
		if (data.southEdgeProcessed) {
			auto neighbour = maskCache.find(pos + Vec2Int{0, 1});
			if (neighbour != maskCache.end()) {
				correct(pos, data, neighbour->second, Edge::South);
			}
		}
	}
}

void BlenderImpl::generateChunkData(Vec2Int chunkPos, float* buffer) {
	if (!buffer) {
		return;
	}

	std::lock_guard lock(maskMutex);

	for (int32_t y = -1; y <= 1; ++y) {
		for (int32_t x = -1; x <= 1; ++x) {
			getChunkData(chunkPos + Vec2Int{x, y});
		}
	}

	for (int32_t y = -1; y <= 1; ++y) {
		for (int32_t x = -1; x < 1; ++x) {
			auto& left = getChunkData(chunkPos + Vec2Int{x, y});
			if (!left.eastEdgeProcessed) {
				auto& right = getChunkData(chunkPos + Vec2Int{x + 1, y});
				smoothEdge(left, right, Edge::East);
				left.eastEdgeProcessed = true;
			}
		}
	}

	for (int32_t y = -1; y < 1; ++y) {
		for (int32_t x = -1; x <= 1; ++x) {
			auto& north = getChunkData(chunkPos + Vec2Int{x, y});
			if (!north.southEdgeProcessed) {
				auto& south = getChunkData(chunkPos + Vec2Int{x, y + 1});
				smoothEdge(north, south, Edge::South);
				north.southEdgeProcessed = true;
			}
		}
	}

	auto& target = getChunkData(chunkPos);
	const int32_t resolution = settings.resolution;
	const int32_t last = resolution - 1;
	const int32_t inner = resolution - radius;
	const int32_t stride = std::max(resolution - 1, 1);
	const Vec2Int origin = chunkPos * stride;
	auto sample = [this, chunkPos](Vec2Int offset, int32_t x, int32_t y, int32_t regionX, int32_t regionY) {
		return CornerSample{&getChunkData(chunkPos + offset), x, y, regionX, regionY};
	};
	auto correctCornerGroup = [this, &sample, last, inner](Corner corner, Vec2Int vertex) {
		std::array<CornerSample, 4> samples;
		switch (corner) {
		case Corner::NorthEast:
			samples = {
				sample({0, -1}, last, last, inner, inner),
				sample({1, -1}, 0, last, 0, inner),
				sample({0, 0}, last, 0, inner, 0),
				sample({1, 0}, 0, 0, 0, 0)
			};
			break;
		case Corner::SouthEast:
			samples = {
				sample({0, 0}, last, last, inner, inner),
				sample({1, 0}, 0, last, 0, inner),
				sample({0, 1}, last, 0, inner, 0),
				sample({1, 1}, 0, 0, 0, 0)
			};
			break;
		case Corner::SouthWest:
			samples = {
				sample({-1, 0}, last, last, inner, inner),
				sample({0, 0}, 0, last, 0, inner),
				sample({-1, 1}, last, 0, inner, 0),
				sample({0, 1}, 0, 0, 0, 0)
			};
			break;
		case Corner::NorthWest:
			samples = {
				sample({-1, -1}, last, last, inner, inner),
				sample({0, -1}, 0, last, 0, inner),
				sample({-1, 0}, last, 0, inner, 0),
				sample({0, 0}, 0, 0, 0, 0)
			};
			break;
		}
		correctCorner(samples, vertex);
	};

	correctCornerGroup(Corner::NorthEast, {origin.x + stride, origin.y});
	correctCornerGroup(Corner::SouthEast, {origin.x + stride, origin.y + stride});
	correctCornerGroup(Corner::SouthWest, {origin.x, origin.y + stride});
	correctCornerGroup(Corner::NorthWest, {origin.x, origin.y});
	correctSharedBoundaries();

	for (int32_t x = 0; x < resolution; ++x) {
		for (int32_t y = 0; y < resolution; ++y) {
			const size_t index = static_cast<size_t>(x) * resolution + y;
			if (x < radius || x >= resolution - radius ||
				y < radius || y >= resolution - radius) {
				buffer[index] = target.heights[index] + target.mask[maskIndex(x, y, resolution, radius)];
			}
			else {
				buffer[index] = target.heights[index];
			}
		}
	}
}

float BlenderImpl::getHeight(Vec2Int pos) {
	const Vec2Int chunkPos = pointToChunk(pos);
	const int32_t stride = std::max(settings.resolution - 1, 1);
	const Vec2Int local = pos - chunkPos * stride;
	std::vector<float> chunk(static_cast<size_t>(settings.resolution) * settings.resolution);
	generateChunkData(chunkPos, chunk.data());
	return chunk[static_cast<size_t>(local.x) * settings.resolution + local.y];
}

void BlenderImpl::clearAllCache() {
	GeneratorImpl::clearAllCache();
	{
		std::lock_guard lock(maskMutex);
		maskCache.clear();
		processedCorners.clear();
	}
}

bool BlenderImpl::isDeterministic() {
	return source->isDeterministic();
}
