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

	EXPECT_NE(dlsym(lib, "getNewContext"), nullptr);
	EXPECT_NE(dlsym(lib, "closeContext"), nullptr);
	EXPECT_NE(dlsym(lib, "setGenerator"), nullptr);
	EXPECT_NE(dlsym(lib, "generateRegion"), nullptr);

	EXPECT_EQ(dlclose(lib), 0);
}
#endif

TEST(CoreTests, HelloWorld)
{
	const std::string hw = "HelloWorld\n";
	EXPECT_EQ(smokeTest(hw.data()), hw);
}

TEST(CoreTests, SetAnyGenerator)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f, nullptr));
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f, nullptr));
	EXPECT_FALSE(setGenerator(ctx, GeneratorType::empty, 1.f, nullptr));
	EXPECT_FALSE(setGenerator(ctx, GeneratorType::generatorTypeSize, 1.f, nullptr));
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f, nullptr));
}

TEST(CoreTests, GenerateAndGetRegion)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f, nullptr));
	Chunk* data = nullptr;
	EXPECT_TRUE(generateRegion(ctx,1,1));
	EXPECT_TRUE(getRegion(ctx,1,1, data));
	EXPECT_NE(data, nullptr);
}

TEST(CoreTests, ReadNonExistantRegion)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f, nullptr));
	Chunk* data = nullptr;
	EXPECT_FALSE(getRegion(ctx,1,1, data));
	EXPECT_EQ(data, nullptr);
}

TEST(CoreTests, SetAmplitude)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setVerticalAmplitude(ctx,1));
	EXPECT_TRUE(setVerticalAmplitude(ctx,0.5));
	EXPECT_TRUE(setVerticalAmplitude(ctx,0));
	EXPECT_TRUE(setVerticalAmplitude(ctx,-1.23));
}

TEST(CoreTests, SetResolution)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setResolution(ctx, 1));
	EXPECT_TRUE(setResolution(ctx, 100));
	EXPECT_FALSE(setResolution(ctx, 0));
}

TEST(GeneratorsTest, TestAllGenerators)
{
	for (int i = static_cast<int>(GeneratorType::empty) + 1; i < static_cast<int>(GeneratorType::generatorTypeSize); i
	     ++)
	{
		HGContextHandle ctx = getNewContext();
		EXPECT_TRUE(setGenerator(ctx, static_cast<GeneratorType>(i), 1.f, nullptr));
		Chunk* data = nullptr;
		EXPECT_FALSE(getRegion(ctx,1,1, data));
		EXPECT_EQ(data, nullptr);
		EXPECT_TRUE(generateRegion(ctx,1,1));
		EXPECT_TRUE(getRegion(ctx,1,1, data));
	}
}

int main(int argc, char** argv)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setRegionDimensions(ctx, 2,2));
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::brownian_perlin, 5.0f, nullptr));
	EXPECT_TRUE(generateRegion(ctx,1,1));
	EXPECT_TRUE(generateRegion(ctx,1,2));

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
