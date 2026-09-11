#include "HeightmapGenAPI.h"
#include <gtest/gtest.h>
#include "onnxruntime_cxx_api.h"

TEST(CoreTests, HelloWorld)
{
    const std::string hw = "HelloWorld\n";
    EXPECT_EQ(smokeTest(hw.data()), hw);
}

TEST(CoreTests, ContextAndGeneratorImplLifecycle)
{
    Context ctx = CreateContext();
    EXPECT_NE(ctx, nullptr);

    CommonSettings commonSettings{0, 1.0f, 1.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    GeneratorHandle baseGen = CreateBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    DestroyGenerator(gen);
    DestroyContext(ctx);
}

TEST(CoreTests, SynchronousDataAccess)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{42, 1.0f, 5.0f, 16, false};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    GeneratorHandle baseGen = CreateBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    float* chunkData = GetChunk(gen, 1, 1);
    EXPECT_NE(chunkData, nullptr);

    float pointSample = GetPoint(gen, 10, 10);
    (void)pointSample;

    DestroyContext(ctx);
}

TEST(CoreTests, CachingAndProbing)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{123, 1.0f, 10.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    GeneratorHandle baseGen = CreateBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    bool ready = false;

    EXPECT_EQ(ProbeChunk(gen, 1, 1, &ready), nullptr);
    EXPECT_FALSE(ready);

    RequestChunk(gen, 1, 1);

    float* cachedData = ProbeChunk(gen, 1, 1, &ready);
    EXPECT_TRUE(ready);
    EXPECT_NE(cachedData, nullptr);

    CleanChunkFromCache(gen, 1, 1);
    EXPECT_EQ(ProbeChunk(gen, 1, 1, &ready), nullptr);
    EXPECT_FALSE(ready);

    DestroyContext(ctx);
}

TEST(CoreTests, NonCacheableProbingBehavior)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{123, 1.0f, 10.0f, 16, false};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    GeneratorHandle baseGen = CreateBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    bool ready = true;
    RequestChunk(gen, 1, 1);

    EXPECT_EQ(ProbeChunk(gen, 1, 1, &ready), nullptr);
    EXPECT_FALSE(ready);

    DestroyContext(ctx);
}

TEST(CoreTests, ClearAllCache)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{777, 1.0f, 1.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    GeneratorHandle baseGen = CreateBrownianPerlinGenerator(ctx, baseSettings);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);

    RequestChunk(gen, 0, 0);
    RequestChunk(gen, 1, 1);

    bool ready0 = false, ready1 = false;
    EXPECT_NE(ProbeChunk(gen, 0, 0, &ready0), nullptr);
    EXPECT_NE(ProbeChunk(gen, 1, 1, &ready1), nullptr);
    EXPECT_TRUE(ready0 && ready1);

    ClearAllCache(gen);

    EXPECT_EQ(ProbeChunk(gen, 0, 0, &ready0), nullptr);
    EXPECT_FALSE(ready0);
    EXPECT_EQ(ProbeChunk(gen, 1, 1, &ready1), nullptr);
    EXPECT_FALSE(ready1);

    DestroyContext(ctx);
}

TEST(OrtModelTests, BPModelSmokeTest)
{
    Context ctx = CreateContext();
    // not really caring about anything other than cache
    // resolution will be overriden based on model anyway
    CommonSettings commonSettings{};
    commonSettings.cacheable = true;
    
    Ort::Env env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "OrtEnv");
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
    const char* modelPath = "../../models/best_step6126.onnx";

    GeneratorHandle gen = CreateBPGenerator(ctx, commonSettings, env, modelPath, sessionOptions);
    EXPECT_NE(gen, nullptr);

    float* chunkData = GetChunk(gen, 1, 1);
    EXPECT_NE(chunkData, nullptr);

    float pointSample = GetPoint(gen, 10, 10);
    (void)pointSample;

    DestroyGenerator(gen);
    DestroyContext(ctx);
}

int main(int argc, char** argv)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{1, 5.0f, 1.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    GeneratorHandle baseGen = CreateBrownianPerlinGenerator(ctx, baseSettings);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    if (gen)
    {
        RequestChunk(gen, 1, 1);
        RequestChunk(gen, 1, 2);
    }

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    DestroyContext(ctx);
    return result;
}