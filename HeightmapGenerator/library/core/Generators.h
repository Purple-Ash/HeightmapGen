#pragma once
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
	Generator(float scaleHorizontal, float amplitude);
	virtual ~Generator() = default;

	virtual float getHeight(float posX, float posY) = 0;
	virtual bool isDeterministic() = 0;
};

class RandomGenerator : public Generator
{
public:
	RandomGenerator(float scaleHorizontal, float amplitude) : Generator(scaleHorizontal, amplitude){};
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};


class PerlinGenerator final : public Generator
{
public:
	PerlinGenerator(float scaleHorizontal, float amplitude);
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};

class BrownianPerlinGenerator final : public Generator
{
	std::vector<PerlinGenerator> octaves;
public:
	BrownianPerlinGenerator(float scaleHorizontal, float amplitude);
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};


