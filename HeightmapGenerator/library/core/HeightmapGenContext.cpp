#include "HeightmapGenContext.h"

Vec2Int::Vec2Int(int32_t x, int32_t y)
{
	this->x = x;
	this->y = y;
}

Chunk::Chunk(Vec2Int size)
{
	this->size = size;
	data = new float[size.x * size.y];
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
		auto [it, inserted] = chunks.try_emplace(position, chunkDimensions);

		if (!inserted) return false;

		Chunk& newChunk = it->second;

		for (int i = 0; i < chunkDimensions.x; i++)
		{
			for (int j = 0; j < chunkDimensions.y; j++)
			{
				newChunk.data[i + j * chunkDimensions.x] = generator->getHeight(i, j);
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

bool HGContext::setGenerator(GeneratorType type, float scale_horizontal, float scale_vertical)
{
	switch (type)
	{
		case GeneratorType::empty:
			return false;
			break;
		case GeneratorType::random:
			generator = new RandomGenerator(scale_horizontal, scale_vertical);
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

