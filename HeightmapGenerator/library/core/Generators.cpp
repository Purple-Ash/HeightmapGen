#include "Generators.h"
#include "Helpers.h"
#include <cstdlib>

Generator::Generator(float scaleHorizontal, float scaleVertical, uint32_t resolution)
{
	this->scaleHorizontal = scaleHorizontal;
	this->amplitude = scaleVertical;
	this->resolution = resolution;
}

void Generator::getHeightmap(float* heightmap, int32_t width, int32_t height, float originX, float originY) {
	for (int x = 0; x < height; x++) {
		for (int y = 0; y < width; y++) {
			float posX = originX + static_cast<float>(x) / static_cast<float>(resolution);
			float posY = originY + static_cast<float>(y) / static_cast<float>(resolution);
			heightmap[x * height + y] = getHeight(posX, posY);
		}
	}
}

float RandomGenerator::getHeight(float posX, float posY)
{
	return randomFloatBetween(0.f,1.f) * amplitude;
}

bool RandomGenerator::isDeterministic()
{
	return false;
}

float CoordinateGenerator::getHeight(float posX, float posY)
{
	return (posX + posY) * amplitude;
}

bool CoordinateGenerator::isDeterministic()
{
	return true;
}