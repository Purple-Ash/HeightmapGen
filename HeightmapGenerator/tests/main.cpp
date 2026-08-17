#include "HeightmapGenAPI.h"
#include <gtest/gtest.h>

#include "../library/core/HeightmapGenContext.h"

#ifdef __linux__
#include <dlfcn.h>
#include <string>

TEST(CoreTests, DynamicLoad)
{
    void* lib = dlopen("libHeightmapGen.so", RTLD_NOW);
    EXPECT_NE(lib, nullptr);

    void* symbol = dlsym(lib, "smokeTest");
    EXPECT_NE(symbol, nullptr);

    auto function = reinterpret_cast<const char*(*)(const char*)>(symbol);

    const char* hw = "HelloWorld\n";
    EXPECT_EQ(function(hw), hw);

    // Verify exported C API functions
    EXPECT_NE(dlsym(lib, "CreateContext"), nullptr);
    EXPECT_NE(dlsym(lib, "DestroyContext"), nullptr);
    EXPECT_NE(dlsym(lib, "CreateHydraulicErosionGeneratorImpl"), nullptr);
    EXPECT_NE(dlsym(lib, "DestroyGeneratorImpl"), nullptr);
    EXPECT_NE(dlsym(lib, "GetChunk"), nullptr);
    EXPECT_NE(dlsym(lib, "GetPoint"), nullptr);
    EXPECT_NE(dlsym(lib, "RequestChunk"), nullptr);
    EXPECT_NE(dlsym(lib, "RequestPoint"), nullptr);
    EXPECT_NE(dlsym(lib, "ProbeChunk"), nullptr);
    EXPECT_NE(dlsym(lib, "ProbePoint"), nullptr);
    EXPECT_NE(dlsym(lib, "CleanChunkFromCache"), nullptr);
    EXPECT_NE(dlsym(lib, "ClearAllCache"), nullptr);

    EXPECT_EQ(dlclose(lib), 0);
}
#endif

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
    HydraulicErosionSettings erosionSettings{};

    GeneratorHandle gen = CreateHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
    EXPECT_NE(gen, nullptr);

    DestroyGenerator(gen);
    DestroyContext(ctx);
}

TEST(CoreTests, SynchronousDataAccess)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{42, 1.0f, 5.0f, 16, false};
    HydraulicErosionSettings erosionSettings{};

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
    HydraulicErosionSettings erosionSettings{};

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
    HydraulicErosionSettings erosionSettings{};

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
    HydraulicErosionSettings erosionSettings{};

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

int main(int argc, char** argv)
{
    Context ctx = CreateContext();
    CommonSettings commonSettings{1, 5.0f, 1.0f, 16, true};
    HydraulicErosionSettings erosionSettings{};

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