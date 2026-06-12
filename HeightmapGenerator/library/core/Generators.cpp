#include "Generators.h"
#include "Helpers.h"
#include <cstdlib>

Generator::Generator(float scaleHorizontal, float scaleVertical)
{
	this->scaleHorizontal = scaleHorizontal;
	this->amplitude = scaleVertical;
}


float RandomGenerator::getHeight(float posX, float posY)
{
	return randomFloatBetween(0.f,1.f) * amplitude;
}

bool RandomGenerator::isDeterministic()
{
	return false;
}