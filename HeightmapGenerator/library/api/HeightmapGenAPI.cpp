#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include "Generators.h"
#include <algorithm>

HG_API const char* smokeTest(const char* data) {
    return data;
}

HG_API Context createContext() {
    return new ContextImpl();
}

HG_API void destroyContext(Context ctx) {
    if (ctx) delete ctx;
}

HG_API Generator createPerlinGenerator(Context ctx, CommonSettings commonSettings) {
    if (!ctx) return nullptr;
    Generator gen = new PerlinGeneratorImpl(ctx, commonSettings);
    ctx->generators.push_back(gen);
    return gen;
}

HG_API Generator createBrownianPerlinGenerator(Context ctx, CommonSettings commonSettings) {
    if (!ctx) return nullptr;
    Generator gen = new BrownianPerlinGeneratorImpl(ctx, commonSettings);
    ctx->generators.push_back(gen);
    return gen;
}

HG_API Generator createHydraulicErosionGenerator(Context ctx, CommonSettings commonSettings, HydraulicErosionSettings erosionSettings) {
    if (!ctx || !erosionSettings.baseGeneratorImpl) return nullptr;
    Generator gen = new HydraulicErosionGeneratorImpl(ctx, commonSettings, erosionSettings);
    ctx->generators.push_back(gen);
    return gen;
}

HG_API void destroyGenerator(Generator generator) {
    if (generator) {
        if (generator->getContext()) {
            auto& gens = generator->getContext()->generators;
            gens.erase(std::remove(gens.begin(), gens.end(), generator), gens.end());
        }
        delete generator;
    }
}

HG_API void getChunk(Generator generator, int32_t x, int32_t y, float* buffer) {
    if (generator) generator->getChunk({ x, y }, buffer);
}

HG_API float getPoint(Generator generator, int32_t x, int32_t y) {
    return generator ? generator->getPoint({ x, y }) : 0.0f;
}

HG_API void requestChunk(Generator generator, int32_t x, int32_t y) {
    if (generator) generator->requestChunk({ x, y });
}

HG_API void requestPoint(Generator generator, int32_t x, int32_t y) {
    if (generator) generator->requestPoint({ x, y });
}

HG_API bool probeChunk(Generator generator, int32_t x, int32_t y, float* buffer) {
    return generator ? generator->probeChunk({ x, y }, buffer) : false;
}

HG_API bool probePoint(Generator generator, int32_t x, int32_t y, float* point) {
    return generator ? generator->probePoint({ x, y }, point) : false;
}

HG_API void cleanChunkFromCache(Generator generator, int32_t x, int32_t y) {
    if (generator) generator->cleanChunkFromCache({ x, y });
}

HG_API void clearAllCache(Generator generator) {
    if (generator) generator->clearAllCache();
}