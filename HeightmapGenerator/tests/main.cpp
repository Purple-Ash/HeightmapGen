#include "HeightmapGenAPI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(CoreTests, HelloWorld)
{
	const std::string hw = "HelloWorld\n";
	EXPECT_STREQ(smokeTest(hw.data()), hw.c_str());
}

TEST(CoreTests, ContextAndGeneratorLifecycle)
{
	Context ctx = createContext(0);
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
	Context ctx = createContext(0);
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
	Context ctx = createContext(0);
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
	Context ctx = createContext(0);
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
	Context ctx = createContext(0);
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

struct TestVec2Int {
	int32_t x;
	int32_t y;
};

TestVec2Int pointToChunk(TestVec2Int point, const CommonSettings& settings) {
	int32_t stride = std::max(settings.resolution - 1, 1);
	return TestVec2Int{
		((point.x >= 0) ? point.x : (point.x - stride + 1)) / stride,
		((point.y >= 0) ? point.y : (point.y - stride + 1)) / stride
	};
}

TEST(CoreTests, GetChunks5by5_Using_getChunk_and_getPoint){
	constexpr int resolution = 5;
	const int stride = resolution - 1;
	Context ctx = createContext(0);
	CommonSettings commonSettings{777, 1.0f, 1.0f, resolution, true};
	Generator baseGen = createBrownianPerlinGenerator(ctx, commonSettings);

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
			TestVec2Int chunk = pointToChunk({x, y}, commonSettings);

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
	destroyContext(ctx);
}

TEST(CoreTests, HydraulicErosionGeneratorTest) {
    Context ctx = createContext(0);
    CommonSettings commonSettings{1, 5.0f, 1.0f, 16, true};

	CommonSettings baseSettings = commonSettings;
	baseSettings.amplitude = 1.0f;
	Generator baseGen = createBrownianPerlinGenerator(ctx, baseSettings);

	HydraulicErosionSettings erosionSettings{};
	erosionSettings.baseGeneratorImpl = baseGen;

	Generator gen = createHydraulicErosionGenerator(ctx, commonSettings, erosionSettings);
	if (gen) {
		requestChunk(gen, 1, 1);
		requestChunk(gen, 1, 2);
	}

	destroyContext(ctx);
}

namespace {

size_t pointIndex(int32_t x, int32_t y, int32_t resolution) {
	return static_cast<size_t>(x) * resolution + y;
}

}

TEST(BlenderTests, BlendsCardinalEdgesAndFourWayCorners) {
	Context ctx = createContext(0);
	constexpr int32_t resolution = 8;
	CommonSettings settings{12, 1.0f, 1.0f, resolution, false};
	Generator source = createRandomGenerator(ctx, settings);
	ASSERT_NE(source, nullptr);
	Generator blender = createBlender(ctx, source, 2);
	ASSERT_NE(blender, nullptr);

	std::vector<float> center(resolution * resolution);
	std::vector<float> east(resolution * resolution);
	std::vector<float> north(resolution * resolution);
	std::vector<float> northEast(resolution * resolution);
	std::vector<float> west(resolution * resolution);
	std::vector<float> south(resolution * resolution);
	std::vector<float> northWest(resolution * resolution);
	std::vector<float> southWest(resolution * resolution);
	std::vector<float> southEast(resolution * resolution);
	getChunk(blender, 0, 0, center.data());
	getChunk(blender, 1, 0, east.data());
	getChunk(blender, 0, -1, north.data());
	getChunk(blender, 1, -1, northEast.data());
	getChunk(blender, -1, 0, west.data());
	getChunk(blender, 0, 1, south.data());
	getChunk(blender, -1, -1, northWest.data());
	getChunk(blender, -1, 1, southWest.data());
	getChunk(blender, 1, 1, southEast.data());
	EXPECT_FLOAT_EQ(getPoint(blender, 3, 4), center[pointIndex(3, 4, resolution)]);

	auto expectSame = [](float a, float b) {
		EXPECT_NEAR(a, b, 1e-5f);
	};

	for (int32_t y = 0; y < resolution; ++y) {
		expectSame(center[pointIndex(resolution - 1, y, resolution)], east[pointIndex(0, y, resolution)]);
		expectSame(center[pointIndex(0, y, resolution)], west[pointIndex(resolution - 1, y, resolution)]);
	}
	for (int32_t x = 0; x < resolution; ++x) {
		expectSame(center[pointIndex(x, 0, resolution)], north[pointIndex(x, resolution - 1, resolution)]);
		expectSame(center[pointIndex(x, resolution - 1, resolution)], south[pointIndex(x, 0, resolution)]);
	}

	const std::array<float, 4> corners{
		center[pointIndex(resolution - 1, 0, resolution)],
		center[pointIndex(resolution - 1, resolution - 1, resolution)],
		center[pointIndex(0, resolution - 1, resolution)],
		center[pointIndex(0, 0, resolution)]
	};
	const std::array<std::array<float, 3>, 4> neighborCorners{{
		{east[pointIndex(0, 0, resolution)], north[pointIndex(resolution - 1, resolution - 1, resolution)],
			northEast[pointIndex(0, resolution - 1, resolution)]},
		{east[pointIndex(0, resolution - 1, resolution)], south[pointIndex(resolution - 1, 0, resolution)],
			southEast[pointIndex(0, 0, resolution)]},
		{west[pointIndex(resolution - 1, resolution - 1, resolution)], south[pointIndex(0, 0, resolution)],
			southWest[pointIndex(resolution - 1, 0, resolution)]},
		{north[pointIndex(0, resolution - 1, resolution)], west[pointIndex(resolution - 1, 0, resolution)],
			northWest[pointIndex(resolution - 1, resolution - 1, resolution)]}
	}};
	for (size_t corner = 0; corner < corners.size(); ++corner) {
		for (float neighborCorner : neighborCorners[corner]) {
			expectSame(corners[corner], neighborCorner);
		}
	}

	destroyGenerator(blender);
	destroyGenerator(source);
	destroyContext(ctx);
}

TEST(BlenderTests, UsesConfiguredRadiusToChooseEdgeHeight) {
	Context ctx = createContext(0);
	constexpr int32_t resolution = 8;
	constexpr int32_t samplingRadius = 3;
	CommonSettings settings{12, 1.0f, 1.0f, resolution, true};
	Generator source = createRandomGenerator(ctx, settings);
	ASSERT_NE(source, nullptr);

	std::vector<float> rawLeft(resolution * resolution);
	std::vector<float> rawRight(resolution * resolution);
	std::vector<float> rawBottom(resolution * resolution);
	std::vector<float> rawBottomRight(resolution * resolution);
	getChunk(source, 0, 0, rawLeft.data());
	getChunk(source, 1, 0, rawRight.data());
	getChunk(source, 0, 1, rawBottom.data());
	getChunk(source, 1, 1, rawBottomRight.data());

	Generator blender = createBlender(ctx, source, 2, samplingRadius);
	ASSERT_NE(blender, nullptr);
	std::vector<float> blendedLeft(resolution * resolution);
	std::vector<float> blendedRight(resolution * resolution);
	getChunk(blender, 0, 0, blendedLeft.data());
	getChunk(blender, 1, 0, blendedRight.data());

	const int32_t alongEdge = 3;
	float expected = 0.0f;
	for (int32_t depth = 0; depth < samplingRadius; ++depth) {
		expected += rawLeft[pointIndex(resolution - depth - 1, alongEdge, resolution)];
		expected += rawRight[pointIndex(depth, alongEdge, resolution)];
	}
	expected /= static_cast<float>(2 * samplingRadius);

	EXPECT_NEAR(blendedLeft[pointIndex(resolution - 1, alongEdge, resolution)], expected, 1e-5f);
	EXPECT_NEAR(blendedRight[pointIndex(0, alongEdge, resolution)], expected, 1e-5f);

	const float edgeOnlyAverage = (
		rawLeft[pointIndex(resolution - 1, alongEdge, resolution)] +
		rawRight[pointIndex(0, alongEdge, resolution)]
	) / 2.0f;
	EXPECT_GT(std::abs(expected - edgeOnlyAverage), 1e-5f);

	float expectedCorner = 0.0f;
	for (int32_t x = 0; x < samplingRadius; ++x) {
		for (int32_t y = 0; y < samplingRadius; ++y) {
			expectedCorner += rawLeft[pointIndex(resolution - samplingRadius + x, resolution - samplingRadius + y, resolution)];
			expectedCorner += rawRight[pointIndex(x, resolution - samplingRadius + y, resolution)];
			expectedCorner += rawBottom[pointIndex(resolution - samplingRadius + x, y, resolution)];
			expectedCorner += rawBottomRight[pointIndex(x, y, resolution)];
		}
	}
	expectedCorner /= static_cast<float>(4 * samplingRadius * samplingRadius);
	EXPECT_NEAR(blendedLeft[pointIndex(resolution - 1, resolution - 1, resolution)], expectedCorner, 1e-5f);

	destroyGenerator(blender);
	destroyGenerator(source);
	destroyContext(ctx);
}

TEST(BlenderTests, SupportsStandardGeneratorOperations) {
	Context ctx = createContext(0);
	CommonSettings settings{12, 1.0f, 1.0f, 8, true};
	Generator source = createCoordinateGenerator(ctx, settings);
	ASSERT_NE(source, nullptr);
	Generator blender = createBlender(ctx, source, 2);
	ASSERT_NE(blender, nullptr);

	std::vector<float> output(64);
	EXPECT_FALSE(probeChunk(blender, 2, -1, output.data()));
	requestChunk(blender, 2, -1);
	ASSERT_TRUE(probeChunk(blender, 2, -1, output.data()));

	std::vector<float> fetched(64);
	getChunk(blender, 2, -1, fetched.data());
	EXPECT_EQ(output, fetched);
	EXPECT_FLOAT_EQ(getPoint(blender, 17, -3), fetched[pointIndex(3, 4, 8)]);

	clearAllCache(blender);
	EXPECT_FALSE(probeChunk(blender, 2, -1, output.data()));

	destroyGenerator(blender);
	destroyGenerator(source);
	destroyContext(ctx);
}

TEST(BlenderTests, MaintainsStableOutputAcrossCacheClear) {
	Context ctx = createContext(0);
	CommonSettings settings{0, 1.0f, 1.0f, 8, true};
	Generator source = createRandomGenerator(ctx, settings);
	ASSERT_NE(source, nullptr);
	Generator blender = createBlender(ctx, source, 2);
	ASSERT_NE(blender, nullptr);

	std::vector<float> first(64);
	std::vector<float> second(64);
	std::vector<float> repeated(64);
	getChunk(blender, 0, 0, first.data());
	getChunk(blender, 1, 0, second.data());
	getChunk(blender, 0, 0, repeated.data());
	EXPECT_EQ(first, repeated);

	clearAllCache(blender);
	getChunk(blender, 0, 0, repeated.data());
	EXPECT_EQ(first, repeated);

	destroyGenerator(blender);
	destroyGenerator(source);
	destroyContext(ctx);
}

TEST(BlenderTests, UsesCachedSourceAfterItsHandleIsDestroyed) {
	Context ctx = createContext(0);
	CommonSettings settings{0, 1.0f, 1.0f, 8, true};
	Generator source = createRandomGenerator(ctx, settings);
	ASSERT_NE(source, nullptr);
	Generator blender = createBlender(ctx, source, 2);
	ASSERT_NE(blender, nullptr);

	std::vector<float> sourceChunk(64);
	for (int32_t y = -1; y <= 1; ++y) {
		for (int32_t x = -1; x <= 1; ++x) {
			getChunk(source, x, y, sourceChunk.data());
		}
	}
	destroyGenerator(source);

	std::vector<float> output(64);
	getChunk(blender, 0, 0, output.data());
	clearAllCache(blender);
	std::vector<float> afterDestroy(64);
	getChunk(blender, 0, 0, afterDestroy.data());
	EXPECT_EQ(output, afterDestroy);
	EXPECT_TRUE(std::all_of(afterDestroy.begin(), afterDestroy.end(), [](float height) {
		return std::isfinite(height);
	}));
	EXPECT_NE(afterDestroy[pointIndex(3, 3, 8)], 0.0f);

	destroyGenerator(blender);
	destroyContext(ctx);
}

TEST(BlenderTests, RejectsInvalidSourceAndRadius) {
	Context ctx = createContext(0);
	Context otherContext = createContext(0);
	CommonSettings settings{0, 1.0f, 1.0f, 8, false};
	Generator source = createCoordinateGenerator(ctx, settings);
	ASSERT_NE(source, nullptr);

	EXPECT_EQ(createBlender(ctx, nullptr, 1), nullptr);
	EXPECT_EQ(createBlender(ctx, source, 0), nullptr);
	EXPECT_EQ(createBlender(ctx, source, 5), nullptr);
	EXPECT_EQ(createBlender(ctx, source, 2, 0), nullptr);
	EXPECT_EQ(createBlender(ctx, source, 2, 9), nullptr);
	EXPECT_EQ(createBlender(otherContext, source, 1), nullptr);

	destroyContext(otherContext);
	destroyGenerator(source);
	destroyContext(ctx);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	int result = RUN_ALL_TESTS();
	return result;
}
