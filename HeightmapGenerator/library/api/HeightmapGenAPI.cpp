#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"

HGContextHandle getNewContext()
{
	return new HGContext();
}

bool closeContext(const HGContextHandle ctx)
{
	if (ctx != nullptr)
	{
		delete ctx;
		return true;
	}
	return false;
}

const char* smokeTest(const char* data)
{
	return data;
}

inline bool setRegionDimensions(HGContextHandle ctx, int32_t sizeX, int32_t sizeY)
{
	return ctx->setRegionDimensions(Vec2Int(sizeX, sizeY));
}

bool setGenerator(HGContextHandle ctx, GeneratorType generator_type, float scaleHorizontal, float scaleVertical)
{
	return ctx->setGenerator(generator_type, scaleHorizontal, scaleVertical);
}

bool generateRegion(HGContextHandle ctx, int32_t x, int32_t y)
{
	return ctx->generateRegion(Vec2Int(x,y));
}

bool getRegion(HGContextHandle ctx, int32_t x, int32_t y, Chunk*& data)
{
	return ctx->getRegion(Vec2Int(x,y), data);
}

