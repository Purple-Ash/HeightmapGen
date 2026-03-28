#include "HeightmapGenAPI.h"
#include <gtest/gtest.h>

TEST(CoreTests, HelloWorld)
{
	const std::string hw = "HelloWorld\n";
	EXPECT_EQ(smokeTest(hw.data()),hw);
}

TEST(CoreTests, SetGenerator)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f,1.f));
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f,1.f));
	EXPECT_FALSE(setGenerator(ctx, GeneratorType::empty, 1.f,1.f));
	EXPECT_FALSE(setGenerator(ctx, GeneratorType::generatorTypeSize, 1.f,1.f));
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f,1.f));
}

TEST(CoreTests, GenerateAndGetRegion)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f,1.f));
	Chunk* data = nullptr;
	EXPECT_TRUE(generateRegion(ctx,1,1));
	EXPECT_TRUE(getRegion(ctx,1,1, data));
	EXPECT_NE(data, nullptr);
}

TEST(CoreTests, ReadNonExistantRegion)
{
	HGContextHandle ctx = getNewContext();
	EXPECT_TRUE(setGenerator(ctx, GeneratorType::random, 1.f,1.f));
	Chunk* data = nullptr;
	EXPECT_FALSE(getRegion(ctx,1,1, data));
	EXPECT_EQ(data, nullptr);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
