#include "Generators.h"
#include "Helpers.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

struct HeightAndGradient {
	float height;
	float gradientX;
	float gradientY;
};

struct BrushPoint {
	int32_t offsetX;
	int32_t offsetY;
	float weight;
};

size_t mapIndex(int32_t x, int32_t y, int32_t height) {
	return static_cast<size_t>(x) * static_cast<size_t>(height) + static_cast<size_t>(y);
}

HeightAndGradient calculateHeightAndGradient(const std::vector<float>& nodes, int32_t mapHeight, float posX, float posY) {
	const auto coordX = static_cast<int32_t>(posX);
	const auto coordY = static_cast<int32_t>(posY);
	const float x = posX - static_cast<float>(coordX);
	const float y = posY - static_cast<float>(coordY);

	const float heightNW = nodes[mapIndex(coordX, coordY, mapHeight)];
	const float heightNE = nodes[mapIndex(coordX + 1, coordY, mapHeight)];
	const float heightSW = nodes[mapIndex(coordX, coordY + 1, mapHeight)];
	const float heightSE = nodes[mapIndex(coordX + 1, coordY + 1, mapHeight)];

	const float gradientX = (heightNE - heightNW) * (1.0f - y) + (heightSE - heightSW) * y;
	const float gradientY = (heightSW - heightNW) * (1.0f - x) + (heightSE - heightNE) * x;
	const float interpolatedHeight = heightNW * (1.0f - x) * (1.0f - y)
		+ heightNE * x * (1.0f - y)
		+ heightSW * (1.0f - x) * y
		+ heightSE * x * y;

	return { interpolatedHeight, gradientX, gradientY };
}

std::vector<BrushPoint> createErosionBrush(int32_t radius) {
	std::vector<BrushPoint> brush;
	const float radiusSquared = static_cast<float>(radius) * static_cast<float>(radius);
	float weightSum = 0.0f;

	for (int32_t x = -radius; x <= radius; x++) {
		for (int32_t y = -radius; y <= radius; y++) {
			const float distanceSquared = static_cast<float>(x) * static_cast<float>(x) + static_cast<float>(y) * static_cast<float>(y);
			if (distanceSquared >= radiusSquared) {
				continue;
			}

			const float weight = 1.0f - std::sqrt(distanceSquared) / radius;
			brush.push_back({ x, y, weight });
			weightSum += weight;
		}
	}

	for (auto& point : brush) {
		point.weight /= weightSum;
	}

	return brush;
}

void erodeHeightmap(std::vector<float>& heightmap, int32_t width, int32_t height, int32_t borderSize,
					int32_t outputSampleCount, const HydraulicErosionSettings& settings) {

	const auto brush = createErosionBrush(settings.erosionRadius);

	const int32_t spawnWidth = width - borderSize * 2 - 1;
	const int32_t spawnHeight = height - borderSize * 2 - 1;
	const int32_t iterationCount = outputSampleCount * settings.numIterations;
	std::mt19937 random(settings.seed);

	for (int32_t iteration = 0; iteration < iterationCount; iteration++) {
		float posX = static_cast<float>(borderSize + static_cast<int32_t>(random() % static_cast<uint32_t>(spawnWidth)));
		float posY = static_cast<float>(borderSize + static_cast<int32_t>(random() % static_cast<uint32_t>(spawnHeight)));
		float dirX = 0.0f;
		float dirY = 0.0f;
		float speed = settings.initialSpeed;
		float water = settings.initialWaterVolume;
		float sediment = 0.0f;

		for (int32_t lifetime = 0; lifetime < settings.maxDropletLifetime; lifetime++) {
			const auto nodeX = static_cast<int32_t>(posX);
			const auto nodeY = static_cast<int32_t>(posY);
			const float cellOffsetX = posX - static_cast<float>(nodeX);
			const float cellOffsetY = posY - static_cast<float>(nodeY);
			const HeightAndGradient oldSample = calculateHeightAndGradient(heightmap, height, posX, posY);

			dirX = dirX * settings.inertia - oldSample.gradientX * (1.0f - settings.inertia);
			dirY = dirY * settings.inertia - oldSample.gradientY * (1.0f - settings.inertia);
			const float directionLength = std::sqrt(dirX * dirX + dirY * dirY);
			if (directionLength <= 0) {
				break;
			}

			dirX /= directionLength;
			dirY /= directionLength;
			posX += dirX;
			posY += dirY;

			if (posX < borderSize || posX >= width - borderSize - 1	|| posY < borderSize || posY >= height - borderSize - 1) {
				break;
			}

			const float newHeight = calculateHeightAndGradient(heightmap, height, posX, posY).height;
			const float deltaHeight = newHeight - oldSample.height;
			const float sedimentCapacity = std::max(
				-deltaHeight * speed * water * settings.sedimentCapacityFactor, settings.minSedimentCapacity);

			if (sediment > sedimentCapacity || deltaHeight > 0.0f) {
				const float amountToDeposit = deltaHeight > 0.0f
					? std::min(deltaHeight, sediment)
					: (sediment - sedimentCapacity) * settings.depositSpeed;
				sediment -= amountToDeposit;

				heightmap[mapIndex(nodeX, nodeY, height)] += amountToDeposit * (1.0f - cellOffsetX) * (1.0f - cellOffsetY);
				heightmap[mapIndex(nodeX + 1, nodeY, height)] += amountToDeposit * cellOffsetX * (1.0f - cellOffsetY);
				heightmap[mapIndex(nodeX, nodeY + 1, height)] += amountToDeposit * (1.0f - cellOffsetX) * cellOffsetY;
				heightmap[mapIndex(nodeX + 1, nodeY + 1, height)] += amountToDeposit * cellOffsetX * cellOffsetY;
			}
			else {
				const float amountToErode = std::min((sedimentCapacity - sediment) * settings.erodeSpeed, -deltaHeight);

				for (const auto& point : brush) {
					const auto index = mapIndex(nodeX + point.offsetX, nodeY + point.offsetY, height);
					const float weightedAmount = amountToErode * point.weight;
					const float availableHeight = std::max(heightmap[index], 0.0f);
					const float deltaSediment = std::min(availableHeight, weightedAmount);
					heightmap[index] -= deltaSediment;
					sediment += deltaSediment;
				}
			}

			const float speedSquared = speed * speed + deltaHeight * settings.gravity;
			speed = std::sqrt(std::max(0.0f, speedSquared));
			water *= 1.0f - settings.evaporateSpeed;
		}
	}
}

HydraulicErosionGenerator::HydraulicErosionGenerator(float scaleHorizontal, float amplitude, uint32_t resolution, const HydraulicErosionSettings* settings) 
	: Generator(scaleHorizontal, amplitude, resolution), hydraulicErosionSettings() {
	if (settings != nullptr) {
		hydraulicErosionSettings = *settings;
	}
}

void HydraulicErosionGenerator::getHeightmap(float* heightmap, int32_t width, int32_t height, float originX, float originY) {
	const int32_t radius = hydraulicErosionSettings.erosionRadius;
	const int32_t paddedWidth = width + radius * 2;
	const int32_t paddedHeight = height + radius * 2;
	const int32_t outputSampleCount = width * height;
	const int32_t paddedSampleCount = paddedWidth * paddedHeight;

	std::vector<float> paddedHeightmap;
	paddedHeightmap.resize(paddedSampleCount);

	const float paddedOriginX = originX - static_cast<float>(radius) / resolution;
	const float paddedOriginY = originY - static_cast<float>(radius) / resolution;

	BrownianPerlinGenerator baseGenerator(scaleHorizontal, 1.0f, resolution);
	baseGenerator.getHeightmap(paddedHeightmap.data(), paddedWidth, paddedHeight, paddedOriginX, paddedOriginY);

	erodeHeightmap(paddedHeightmap, paddedWidth, paddedHeight, radius, outputSampleCount, hydraulicErosionSettings);

	for (int32_t x = 0; x < width; x++) {
		for (int32_t y = 0; y < height; y++) {
			heightmap[x * height + y] = paddedHeightmap[(x + radius) * paddedHeight + (y + radius)] * amplitude;
		}
	}
}

float HydraulicErosionGenerator::getHeight(float posX, float posY) {
	return 0.0f;
}

bool HydraulicErosionGenerator::isDeterministic() {
	return true;
}
