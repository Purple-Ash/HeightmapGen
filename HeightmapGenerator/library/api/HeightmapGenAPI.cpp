#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"

HG_API const char* smokeTest(const char* data)
{
	return data;
}

HG_API HGContextHandle getNewContext()
{
	return new HGContext();
}

HG_API bool closeContext(const HGContextHandle ctx)
{
	if (ctx != nullptr)
	{
		delete ctx;
		return true;
	}
	return false;
}

HG_API bool setRegionDimensions(HGContextHandle ctx, int32_t sizeX, int32_t sizeY)
{
	return ctx->setRegionDimensions(Vec2Int(sizeX, sizeY));
}

HG_API bool setVerticalAmplitude(HGContextHandle ctx, float multiplier)
{
	return ctx->setVerticalAmplitude(multiplier);
}

HG_API bool setResolution(HGContextHandle ctx, int32_t resolution)
{
	return ctx->setResolution(resolution);
}

HG_API bool setGenerator(HGContextHandle ctx, GeneratorType generator_type, float scaleHorizontal)
{
	return ctx->setGenerator(generator_type, scaleHorizontal);
}

HG_API bool generateRegion(HGContextHandle ctx, int32_t x, int32_t y)
{
	return ctx->generateRegion(Vec2Int(x,y));
}

HG_API bool getRegion(HGContextHandle ctx, int32_t x, int32_t y, Chunk*& data)
{
	return ctx->getRegion(Vec2Int(x,y), data);
}
