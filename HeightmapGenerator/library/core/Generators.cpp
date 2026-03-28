#include "Generators.h"
#include "Helpers.h"
#include <cstdlib>

Generator::Generator(float scaleHorizontal, float scaleVertical)
{
	this->scaleHorizontal = scaleHorizontal;
	this->scaleVertical = scaleVertical;
}


float RandomGenerator::getHeight(float posX, float posY)
{
	return randomFloatBetween(1.f,2.f) * scaleVertical;
}

bool RandomGenerator::isDeterministic()
{
	return false;
}
