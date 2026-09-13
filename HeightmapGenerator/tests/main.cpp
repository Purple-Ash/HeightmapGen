#include "HeightmapGenAPI.h"
#include "../library/core/Helpers.h"
#include <gtest/gtest.h>
#include <vector>

#include "../library/core/Generators.h"
#include "../library/core/Generators.cpp"

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

TEST(CoreTests, GetChunks5by5_Using_getChunk_and_getPoint){
    constexpr int resolution = 5;
    const int stride = resolution - 1;
    Context ctx = createContext();
    CommonSettings commonSettings{777, 1.0f, 1.0f, resolution, true};
    Generator baseGen = createBrownianPerlinGenerator(ctx, commonSettings);

    //cast from opaque handle to be able to test pointToChunk
    auto* impl = static_cast<GeneratorImpl*>(baseGen);

    constexpr int radius = 1;
    constexpr int chunksPerSide = radius * 2 + 1;
    constexpr int chunkCount = chunksPerSide * chunksPerSide;
    float buffer[resolution * resolution * chunkCount];

    for(int i = -radius; i <= radius; i++){
        for(int j = -radius; j <= radius; j++){
            const int chunkIndex = (i + radius) + (j + radius) * chunksPerSide;
            getChunk(baseGen,i,j, &buffer[resolution*resolution*chunkIndex]);
        }
    }

    //it could be `x/y <= stride * (radious + 1)` if we checked for existance of cache
    //in neighbouring chunk when checking values right on the border of chunk.
    //we would need to hold reference to 4 potential borders (or 8 if we really want 4 points
    //on the vertices of the chunks)
    for(int y = -stride * radius; y < stride * (radius + 1); y++){
        for(int x = -stride * radius; x < stride * (radius + 1); x++){
            float point = getPoint(baseGen, x, y);
            Vec2Int chunk = impl->pointToChunk({x, y});

            ASSERT_GE(chunk.x, -radius);
            ASSERT_LE(chunk.x, radius);
            ASSERT_GE(chunk.y, -radius);
            ASSERT_LE(chunk.y, radius);

            const int chunkIndex = (chunk.x + radius) + (chunk.y + radius) * chunksPerSide;

            const int localX = x - chunk.x * stride;
            const int localY = y - chunk.y * stride;

            ASSERT_GE(localX, 0);
            ASSERT_LT(localX, resolution);
            ASSERT_GE(localY, 0);
            ASSERT_LT(localY, resolution);

            const int pointIndex = localX * resolution + localY;
            const float chunkPoint = buffer[
                    chunkIndex *
                    resolution *
                    resolution +
                    pointIndex];

            EXPECT_FLOAT_EQ(point, chunkPoint);
        }
    }
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
