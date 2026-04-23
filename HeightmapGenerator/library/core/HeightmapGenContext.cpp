#include "HeightmapGenContext.h"

Vec2Int::Vec2Int(int32_t x, int32_t y)
{
	this->x = x;
	this->y = y;
}

Chunk::Chunk(Vec2Int size, int32_t resolution)
{
	this->size = size;
	resoltuion = resolution;
	data = new float[(size.x+1)*resolution * (size.y+1)*resolution];
}

Chunk::Chunk(Chunk&& other) noexcept: data(other.data), size(other.size) {
	other.data = nullptr;
}

Chunk& Chunk::operator=(Chunk&& other) noexcept{
	if (this != &other) {
		delete[] data;
		data = other.data;
		size = other.size;
		other.data = nullptr;
	}
	return *this;
}

Chunk::~Chunk()
{
	delete[] data;
}

bool HGContext::generateRegion(Vec2Int position)
{
	if (generator == nullptr) return false;
	if (!chunks.contains(position))
	{
		auto [it, inserted] = chunks.try_emplace(position, chunkDimensions, resolution);

		if (!inserted) return false;

		Chunk& newChunk = it->second;

		int max_x = (chunkDimensions.x+1)*resolution;
		int max_y = (chunkDimensions.y+1)*resolution;
		for (int i = 0; i < max_x; i++)
		{
			for (int j = 0; j < max_y; j++)
			{
				newChunk.data[i + j * max_x] = generator->getHeight(
					static_cast<float>(i)/static_cast<float>(resolution),
					static_cast<float>(j)/static_cast<float>(resolution)
					);
			}
		}
		return true;
	}
	return false;
}

bool HGContext::getRegion(Vec2Int position, Chunk*& data)
{
	auto it = chunks.find(position);
	if (it != chunks.end())
	{
		data = &(it->second);
		return true;
	}
	return false;
}


bool HGContext::setVerticalAmplitude(float amplitude)
{
	this->amplitude = amplitude;
	return true;
}

bool HGContext::setResolution(int32_t resolution)
{
	if (resolution > 0)
	{
		this->resolution = resolution;
		return true;
	}
	return false;
}

bool HGContext::setGenerator(GeneratorType type, float scale_horizontal)
{
	switch (type)
	{
		case GeneratorType::empty:
			return false;
			break;
		case GeneratorType::random:
			generator = new RandomGenerator(scale_horizontal, amplitude);
			break;
		case GeneratorType::perlin:
			generator = new PerlinGenerator(scale_horizontal, amplitude);
			break;
		default:
			return false;
	}
	return true;
}

bool HGContext::setRegionDimensions(Vec2Int dimensions)
{
	if (dimensions.x > 0 && dimensions.y > 0)
	{
		this->chunkDimensions = dimensions;
		return true;
	}
	return false;
}

HGContext::~HGContext()
{
	delete generator;
}

