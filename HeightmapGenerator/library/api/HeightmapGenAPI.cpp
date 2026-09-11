#include "HeightmapGenAPI.h"
#include "HeightmapGenContext.h"
#include "Generators.h"
#include <algorithm>

HG_API const char* smokeTest(const char* data) {
    return data;
}

HG_API Context CreateContext() {
    return new ContextImpl();
}

HG_API void DestroyContext(Context ctx) {
    if (ctx) delete ctx;
}

HG_API GeneratorHandle CreatePerlinGenerator(Context ctx, CommonSettings commonSettings) {
    if (!ctx) return nullptr;
    GeneratorHandle gen = new PerlinGeneratorImpl(ctx, commonSettings);
    ctx->GeneratorImpls.push_back(gen);
    return gen;
}

HG_API GeneratorHandle CreateBrownianPerlinGenerator(Context ctx, CommonSettings commonSettings) {
    if (!ctx) return nullptr;
    GeneratorHandle gen = new BrownianPerlinGeneratorImpl(ctx, commonSettings);
    ctx->GeneratorImpls.push_back(gen);
    return gen;
}

HG_API GeneratorHandle CreateHydraulicErosionGenerator(Context ctx, CommonSettings commonSettings, HydraulicErosionSettings erosionSettings) {
    if (!ctx || !erosionSettings.baseGeneratorImpl) return nullptr;
    GeneratorHandle gen = new HydraulicErosionGeneratorImpl(ctx, commonSettings, erosionSettings);
    ctx->GeneratorImpls.push_back((gen));
    return gen;
}

HG_API GeneratorHandle CreateBPGenerator(
    Context ctx, 
    CommonSettings commonSettings,
    const Ort::Env &env,
    const char* modelPath,
    const Ort::SessionOptions &sessionOptions
) {
    if (!ctx || !env) return nullptr;
    GeneratorHandle gen = new BPGeneratorImpl(ctx, commonSettings, env, sessionOptions, modelPath);
    ctx->GeneratorImpls.push_back(gen);
    return gen;
}


HG_API void DestroyGenerator(GeneratorHandle Generator) {
    if (Generator) {
        if (Generator->context) {
            auto& gens = Generator->context->GeneratorImpls;
            gens.erase(std::remove(gens.begin(), gens.end(), Generator), gens.end());
        }
        delete Generator;
    }
}

HG_API float* GetChunk(GeneratorHandle GeneratorImpl, int32_t x, int32_t y) {
    return GeneratorImpl ? GeneratorImpl->GetChunk(x, y) : nullptr;
}

HG_API float GetPoint(GeneratorHandle GeneratorImpl, int32_t x, int32_t y) {
    return GeneratorImpl ? GeneratorImpl->GetPoint(x, y) : 0.0f;
}

HG_API void RequestChunk(GeneratorHandle GeneratorImpl, int32_t x, int32_t y) {
    if (GeneratorImpl) GeneratorImpl->RequestChunk(x, y);
}

HG_API void RequestPoint(GeneratorHandle GeneratorImpl, int32_t x, int32_t y) {
    if (GeneratorImpl) GeneratorImpl->RequestPoint(x, y);
}

HG_API float* ProbeChunk(GeneratorHandle GeneratorImpl, int32_t x, int32_t y, bool* ready) {
    return GeneratorImpl ? GeneratorImpl->ProbeChunk(x, y, ready) : nullptr;
}

HG_API float ProbePoint(GeneratorHandle GeneratorImpl, int32_t x, int32_t y, bool* ready) {
    return GeneratorImpl ? GeneratorImpl->ProbePoint(x, y, ready) : 0.0f;
}

HG_API void CleanChunkFromCache(GeneratorHandle GeneratorImpl, int32_t x, int32_t y) {
    if (GeneratorImpl) GeneratorImpl->CleanChunkFromCache(x, y);
}

HG_API void ClearAllCache(GeneratorHandle GeneratorImpl) {
    if (GeneratorImpl) GeneratorImpl->ClearAllCache();
}