#pragma once
#include "HeightmapGenAPI.h"

#include <cstdint>
#include <vector>

/**
 * Parent class for all other classes that will implement terrain generation
 */
class Generator
{
protected:
	float scaleHorizontal, amplitude, vertexCount;
	uint32_t resolution;

public:
	Generator(float scaleHorizontal, float amplitude, uint32_t resolution);
	virtual ~Generator() = default;

	virtual float getHeight(float posX, float posY) = 0;
	virtual void getHeightmap(float* heightmap, int32_t width, int32_t height, float originX, float originY);
	virtual bool isDeterministic() = 0;
};

class CoordinateGenerator : public Generator {
public:
	CoordinateGenerator(float scaleHorizontal, float amplitude, uint32_t resolution) : Generator(scaleHorizontal, amplitude, resolution) {};
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};

class RandomGenerator : public Generator
{
public:
	RandomGenerator(float scaleHorizontal, float amplitude, uint32_t resolution) : Generator(scaleHorizontal, amplitude, resolution){};
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};

class PerlinGenerator final : public Generator
{
public:
	PerlinGenerator(float scaleHorizontal, float amplitude, uint32_t resolution);
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};

class BrownianPerlinGenerator final : public Generator
{
	std::vector<PerlinGenerator> octaves;
public:
	BrownianPerlinGenerator(float scaleHorizontal, float amplitude, uint32_t resolution);
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};

class HydraulicErosionGenerator final : public Generator
{
	HydraulicErosionSettings hydraulicErosionSettings;
public:
	HydraulicErosionGenerator(float scaleHorizontal, float amplitude, uint32_t resolution, const HydraulicErosionSettings* settings);
	float getHeight(float posX, float posY) override;
	void getHeightmap(float* heightmap, int32_t width, int32_t height, float originX, float originY) override;
	bool isDeterministic() override;
};
