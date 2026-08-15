#include "HeightmapGenContext.h"

Vec2Int::Vec2Int(int32_t x, int32_t y)
{
	this->x = x;
	this->y = y;
}

Chunk::Chunk(Vec2Int size, int32_t resolution, float amplitude)
{
	this->size = size;
	data = new float[(size.x * resolution + 1) * (size.y * resolution + 1)];
	this->amplitude = amplitude;
}

Chunk::Chunk(Chunk&& other) noexcept : data(other.data), size(other.size), amplitude(other.amplitude)
{
	other.data = nullptr;
}

Chunk& Chunk::operator=(Chunk&& other) noexcept
{
	if (this != &other)
	{
		delete[] data;
		data = other.data;
		size = other.size;
		amplitude = other.amplitude;
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
		auto [it, inserted] = chunks.try_emplace(
			position,
			chunkDimensions,
			resolution,
			amplitude
		);

		if (!inserted) return false;

		Chunk& newChunk = it->second;

		int max_x = (chunkDimensions.x) * resolution + 1;
		int max_y = (chunkDimensions.y) * resolution + 1;
		auto region_offset_x = static_cast<float>(position.x * chunkDimensions.x);
		auto region_offset_y = static_cast<float>(position.y * chunkDimensions.y);

		generator->getHeightmap(
			newChunk.data,
			max_x, max_y,
			region_offset_x, region_offset_y
		);

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

bool HGContext::setGenerator(GeneratorType type, float scale_horizontal, void* settings)
{
	Generator* newGenerator = nullptr;
	switch (type)
	{
	case GeneratorType::empty:
		return false;
	case GeneratorType::random:
		newGenerator = new RandomGenerator(scale_horizontal, amplitude, resolution);
		break;
	case GeneratorType::perlin:
		newGenerator = new PerlinGenerator(scale_horizontal, amplitude, resolution);
		break;
	case GeneratorType::brownian_perlin:
		newGenerator = new BrownianPerlinGenerator(scale_horizontal, amplitude, resolution);
		break;
	case GeneratorType::hydraulic_erosion:
	{
		auto* erosionSettings = static_cast<HydraulicErosionSettings*>(settings);
		newGenerator = new HydraulicErosionGenerator(scale_horizontal, amplitude, resolution, erosionSettings);
		break;
	}
	case GeneratorType::coordinate:
		newGenerator = new CoordinateGenerator(scale_horizontal, amplitude, resolution);
		break;
	default:
		return false;
	}

	delete generator;
	generator = newGenerator;
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
