#include "HeightmapGenAPI.h"
#include <gtest/gtest.h>
#include <vector>

TEST(CoreTests, HelloWorld)
{
    const std::string hw = "HelloWorld\n";
    EXPECT_EQ(smokeTest(hw.data()), hw);
}

TEST(CoreTests, ContextAndGeneratorImplLifecycle)
{
    Context ctx = createContext();
    EXPECT_NE(ctx, nullptr);

    CommonSettings commonSettings{0, 1.0f, 1.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    destroyGenerator(gen);
    destroyContext(ctx);
}

TEST(CoreTests, SynchronousDataAccess)
{
    Context ctx = createContext();
    CommonSettings commonSettings{42, 1.0f, 5.0f, 16, false};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    std::vector<float> chunkData(commonSettings.resolution * commonSettings.resolution);
    getChunk(gen, 1, 1, chunkData.data());

    float pointSample = getPoint(gen, 10, 10);
    (void)pointSample;

    destroyContext(ctx);
}

TEST(CoreTests, CachingAndProbing)
{
    Context ctx = createContext();
    CommonSettings commonSettings{123, 1.0f, 10.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    std::vector<float> chunkData(commonSettings.resolution * commonSettings.resolution);
    EXPECT_FALSE(probeChunk(gen, 1, 1, chunkData.data()));

    requestChunk(gen, 1, 1);

    EXPECT_TRUE(probeChunk(gen, 1, 1, chunkData.data()));

    cleanChunkFromCache(gen, 1, 1);
    EXPECT_FALSE(probeChunk(gen, 1, 1, chunkData.data()));

    destroyContext(ctx);
}

TEST(CoreTests, NonCacheableProbingBehavior)
{
    Context ctx = createContext();
    CommonSettings commonSettings{123, 1.0f, 10.0f, 16, false};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);
    EXPECT_NE(baseGen, nullptr);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    std::vector<float> chunkData(commonSettings.resolution * commonSettings.resolution);
    requestChunk(gen, 1, 1);

    EXPECT_FALSE(probeChunk(gen, 1, 1, chunkData.data()));

    destroyContext(ctx);
}

TEST(CoreTests, ClearAllCache)
{
    Context ctx = createContext();
    CommonSettings commonSettings{777, 1.0f, 1.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);

    requestChunk(gen, 0, 0);
    requestChunk(gen, 1, 1);

    std::vector<float> chunkData0(commonSettings.resolution * commonSettings.resolution);
    std::vector<float> chunkData1(commonSettings.resolution * commonSettings.resolution);
    EXPECT_TRUE(probeChunk(gen, 0, 0, chunkData0.data()));
    EXPECT_TRUE(probeChunk(gen, 1, 1, chunkData1.data()));

    clearAllCache(gen);

    EXPECT_FALSE(probeChunk(gen, 0, 0, chunkData0.data()));
    EXPECT_FALSE(probeChunk(gen, 1, 1, chunkData1.data()));

    destroyContext(ctx);
}

int main(int argc, char** argv)
{
    Context ctx = createContext();
    CommonSettings commonSettings{1, 5.0f, 1.0f, 16, true};

    CommonSettings baseSettings = commonSettings;
    baseSettings.amplitude = 1.0f;
    Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);

    HydraulicErosionSettings erosionSettings{};
    erosionSettings.baseGeneratorImpl = baseGen;

    Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    if (gen)
    {
        requestChunk(gen, 1, 1);
        requestChunk(gen, 1, 2);
    }

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    destroyContext(ctx);
    return result;
}
