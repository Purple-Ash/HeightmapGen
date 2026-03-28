#pragma once

/**
 * Parent class for all other classes that will implement terrain generation
 */
class Generator
{
protected:
	float scaleHorizontal, scaleVertical;
public:
	Generator(float scaleHorizontal, float scaleVertical);
	virtual ~Generator() = default;

	virtual float getHeight(float posX, float posY) = 0;
	virtual bool isDeterministic() = 0;
};

class RandomGenerator : public Generator
{
public:
	RandomGenerator(float scaleHorizontal, float scaleVertical) : Generator(scaleHorizontal, scaleVertical){};
	float getHeight(float posX, float posY) override;
	bool isDeterministic() override;
};


