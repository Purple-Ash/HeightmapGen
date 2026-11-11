#include "HeightmapGenAPI.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "../library/core/Generators.h"

TEST(CoreTests, HelloWorld)
{
	const std::string hw = "HelloWorld\n";
	EXPECT_EQ(smokeTest(hw.data()), hw);
}

TEST(CoreTests, ContextAndGeneratorImplLifecycle)
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

// TODO: Make the non-exported core functions available for testing.
Vec2Int pointToChunk(Vec2Int point, const CommonSettings& settings) {
	int32_t stride = std::max(settings.resolution - 1, 1);
	return Vec2Int(
		((point.x >= 0) ? point.x : (point.x - stride + 1)) / stride,
		((point.y >= 0) ? point.y : (point.y - stride + 1)) / stride
	);
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
			Vec2Int chunk = pointToChunk({x, y}, commonSettings);

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
	CommonSettings commonSettings{ 1, 5.0f, 1.0f, 16, true };

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

TEST(CoreTests, KnownGeneratorsReturnTheSameChunkTwice)
{
	int resolution = 16;
	Context ctx = createContext(0);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 2026, 1.0f, 1.0f, resolution, false };
	Generator coordinate = createCoordinateGenerator(ctx, settings);
	Generator voronoi = createVoronoiGenerator(ctx, settings);
	Generator perlin = createPerlinGenerator(ctx, settings);
	Generator brownianPerlin = createBrownianPerlinGenerator(ctx, settings);

	HydraulicErosionSettings erosionSettings{};
	erosionSettings.baseGeneratorImpl = brownianPerlin;
	Generator hydraulicErosion = createHydraulicErosionGenerator(ctx, settings, erosionSettings);
	Generator blender = createBlenderGenerator(ctx, settings, BlenderSettings{ 3, 2, hydraulicErosion });

	std::array<std::pair<const char*, Generator>, 6> generators{{
		{ "Coordinate", coordinate },
		{ "Voronoi", voronoi },
		{ "Perlin", perlin },
		{ "Brownian Perlin", brownianPerlin },
		{ "Hydraulic Erosion", hydraulicErosion },
		{ "Blender", blender }
	}};
	for (auto& [name, generator] : generators) {
		ASSERT_NE(generator, nullptr);

		std::vector<float> first(resolution * resolution);
		std::vector<float> second(resolution * resolution);
		ASSERT_NE(first.data(), nullptr);
		ASSERT_NE(second.data(), nullptr);
		EXPECT_NO_THROW({
			getChunk(generator, 1, 1, first.data());
			getChunk(generator, 1, 1, second.data());
		});
		EXPECT_EQ(first, second);
	}

	destroyContext(ctx);
}

TEST(BlenderTests, BlenderOutputIsIndependentOfChunkRequestOrder) {
	Context ctx = createContext(2);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, 16, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);

	BlenderSettings blenderSettings{ 3, 4, base };
	Generator first = createBlenderGenerator(ctx, settings, blenderSettings);
	Generator second = createBlenderGenerator(ctx, settings, blenderSettings);
	ASSERT_NE(first, nullptr);
	ASSERT_NE(second, nullptr);

	std::array<std::array<int, 2>, 5> positions{{
		{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 1}
	}};
	std::vector<std::vector<float>> expected;
	for (auto& pos : positions) {
		std::vector<float> chunk(settings.resolution * settings.resolution);
		getChunk(first, pos[0], pos[1], chunk.data());
		expected.push_back(std::move(chunk));
	}

	for (size_t reverseIndex = positions.size(); reverseIndex > 0; --reverseIndex) {
		size_t index = reverseIndex - 1;
		auto& pos = positions[index];
		std::vector<float> chunk(settings.resolution * settings.resolution);
		getChunk(second, pos[0], pos[1], chunk.data());
		ASSERT_EQ(chunk.size(), expected[index].size());
		for (size_t sample = 0; sample < chunk.size(); ++sample) {
			EXPECT_FLOAT_EQ(chunk[sample], expected[index][sample]);
		}
	}

	destroyContext(ctx);
}

TEST(BlenderTests, BlenderOutputIsStableAfterGeneratingAllNeighbours) {
	int resolution = 16;
	Context ctx = createContext(0);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);
	Generator targetFirstBlender =
		createBlenderGenerator(ctx, settings, BlenderSettings{ 3, 2, base });
	Generator neighboursFirstBlender =
		createBlenderGenerator(ctx, settings, BlenderSettings{ 3, 2, base });
	ASSERT_NE(targetFirstBlender, nullptr);
	ASSERT_NE(neighboursFirstBlender, nullptr);

	std::vector<float> targetFirst(resolution * resolution);
	std::vector<float> targetAfterNeighbours(resolution * resolution);
	getChunk(targetFirstBlender, 0, 0, targetFirst.data());

	std::vector<float> neighbour(resolution * resolution);
	for (int x = -1; x <= 1; ++x) {
		for (int y = -1; y <= 1; ++y) {
			if (x == 0 && y == 0) {
				continue;
			}
			getChunk(neighboursFirstBlender, x, y, neighbour.data());
		}
	}
	getChunk(neighboursFirstBlender, 0, 0, targetAfterNeighbours.data());

	EXPECT_EQ(targetFirst, targetAfterNeighbours);
	destroyContext(ctx);
}

TEST(BlenderTests, BlenderRebuildsChunkAfterCacheClean) {
	int resolution = 16;
	Context ctx = createContext(0);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);
	Generator blender = createBlenderGenerator(ctx, settings, BlenderSettings{ 3, 4, base });
	ASSERT_NE(blender, nullptr);

	std::vector<float> expected(resolution * resolution);
	std::vector<float> rebuilt(resolution * resolution);
	getChunk(blender, 0, 1, expected.data());
	cleanChunkFromCache(blender, 0, 1);
	getChunk(blender, 0, 1, rebuilt.data());
	for (size_t sample = 0; sample < rebuilt.size(); ++sample) {
		EXPECT_FLOAT_EQ(rebuilt[sample], expected[sample]);
	}

	destroyContext(ctx);
}

TEST(BlenderTests, BlenderProducesMatchingSharedEdges) {
	int resolution = 16;
	Context ctx = createContext(2);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);

	BlenderSettings blenderSettings{ 3, 4, base };
	Generator blender = createBlenderGenerator(ctx, settings, blenderSettings);
	ASSERT_NE(blender, nullptr);

	std::vector<float> west(resolution * resolution);
	std::vector<float> east(resolution * resolution);
	std::vector<float> south(resolution * resolution);
	std::vector<float> north(resolution * resolution);
	getChunk(blender, 0, 0, west.data());
	getChunk(blender, 1, 0, east.data());
	getChunk(blender, 0, 1, south.data());
	getChunk(blender, 1, 1, north.data());

	for (int i = 0; i < resolution; ++i) {
		EXPECT_FLOAT_EQ(west[(resolution - 1) * resolution + i], east[i]);
		EXPECT_FLOAT_EQ(south[(resolution - 1) * resolution + i], north[i]);
		EXPECT_FLOAT_EQ(west[i * resolution + resolution - 1], south[i * resolution]);
		EXPECT_FLOAT_EQ(east[i * resolution + resolution - 1], north[i * resolution]);
	}

	destroyContext(ctx);
}

TEST(BlenderTests, BlenderProducesMatchingSharedEdgesForIndependentChunks) {
	int resolution = 16;
	for (int blendingRadius : { 1, 3, 7 }) {
		Context ctx = createContext(0);
		ASSERT_NE(ctx, nullptr);

		CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
		Generator base = createRandomGenerator(ctx, settings);
		ASSERT_NE(base, nullptr);
		Generator blender = createBlenderGenerator(
			ctx, settings, BlenderSettings{ blendingRadius, 1, base });
		ASSERT_NE(blender, nullptr);
		Generator reverseOrderBlender = createBlenderGenerator(
			ctx, settings, BlenderSettings{ blendingRadius, 1, base });
		ASSERT_NE(reverseOrderBlender, nullptr);

		std::vector<float> southwest(resolution * resolution);
		std::vector<float> southeast(resolution * resolution);
		std::vector<float> northwest(resolution * resolution);
		std::vector<float> northeast(resolution * resolution);
		getChunk(blender, 0, 0, southwest.data());
		getChunk(blender, 1, 0, southeast.data());
		getChunk(blender, 0, 1, northwest.data());
		getChunk(blender, 1, 1, northeast.data());

		std::vector<float> reverseChunk(resolution * resolution);
		for (auto& pos : { std::array<int, 2>{ 1, 1 }, { 0, 1 }, { 1, 0 }, { 0, 0 } }) {
			getChunk(reverseOrderBlender, pos[0], pos[1], reverseChunk.data());
			std::vector<float>* expected = nullptr;
			if (pos == std::array<int, 2>{ 0, 0 }) expected = &southwest;
			else if (pos == std::array<int, 2>{ 1, 0 }) expected = &southeast;
			else if (pos == std::array<int, 2>{ 0, 1 }) expected = &northwest;
			else expected = &northeast;
			for (size_t sample = 0; sample < reverseChunk.size(); ++sample) {
				EXPECT_FLOAT_EQ(reverseChunk[sample], (*expected)[sample])
					<< "blendingRadius=" << blendingRadius << ", chunk=("
					<< pos[0] << ", " << pos[1] << "), sample=" << sample;
			}
		}

		for (int i = 0; i < resolution; ++i) {
			EXPECT_FLOAT_EQ(southwest[(resolution - 1) * resolution + i], southeast[i])
				<< "blendingRadius=" << blendingRadius << ", east edge index=" << i;
			EXPECT_FLOAT_EQ(northwest[(resolution - 1) * resolution + i], northeast[i])
				<< "blendingRadius=" << blendingRadius << ", east edge index=" << i;
			EXPECT_FLOAT_EQ(southwest[i * resolution + resolution - 1], northwest[i * resolution])
				<< "blendingRadius=" << blendingRadius << ", south edge index=" << i;
			EXPECT_FLOAT_EQ(southeast[i * resolution + resolution - 1], northeast[i * resolution])
				<< "blendingRadius=" << blendingRadius << ", south edge index=" << i;
		}

		destroyContext(ctx);
	}
}

TEST(BlenderTests, BlenderProducesMatchingSharedCorners) {
	int resolution = 16;
	Context ctx = createContext(2);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);
	Generator blender = createBlenderGenerator(ctx, settings, BlenderSettings{ 3, 4, base });
	ASSERT_NE(blender, nullptr);

	std::vector<float> southwest(resolution * resolution);
	std::vector<float> southeast(resolution * resolution);
	std::vector<float> northwest(resolution * resolution);
	std::vector<float> northeast(resolution * resolution);
	getChunk(blender, 0, 0, southwest.data());
	getChunk(blender, 1, 0, southeast.data());
	getChunk(blender, 0, 1, northwest.data());
	getChunk(blender, 1, 1, northeast.data());

	size_t southwestCorner = (resolution - 1) * resolution + resolution - 1;
	size_t southeastCorner = (resolution - 1) * resolution;
	size_t northwestCorner = resolution - 1;
	EXPECT_FLOAT_EQ(southwest[southwestCorner], southeast[southeastCorner]);
	EXPECT_FLOAT_EQ(southwest[southwestCorner], northwest[northwestCorner]);
	EXPECT_FLOAT_EQ(southwest[southwestCorner], northeast[0]);

	destroyContext(ctx);
}

void expectBlenderSmoothsWithEdgeSamplingRadius(int edgeSamplingRadius) {
	int resolution = 16;
	int blendingRadius = 3;
	int sampleY = resolution / 2;
	size_t chunkArea = resolution * resolution;

	Context ctx = createContext(0);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);

	BlenderSettings blenderSettings{ blendingRadius, edgeSamplingRadius, base };
	Generator blender = createBlenderGenerator(ctx, settings, blenderSettings);
	ASSERT_NE(blender, nullptr);

	std::vector<float> baseChunk(chunkArea);
	std::vector<float> baseNeighbor(chunkArea);
	std::vector<float> blendedChunk(chunkArea);
	getChunk(base, 0, 0, baseChunk.data());
	getChunk(base, 1, 0, baseNeighbor.data());
	getChunk(blender, 0, 0, blendedChunk.data());

	float sharedHeight = 0.0f;
	for (int inward = 0; inward < edgeSamplingRadius; ++inward) {
		sharedHeight += baseChunk[(resolution - 1 - inward) * resolution + sampleY];
		sharedHeight += baseNeighbor[inward * resolution + sampleY];
	}
	sharedHeight /= 2.0f * edgeSamplingRadius;

	for (int depth = 0; depth < blendingRadius; ++depth) {
		int localX = resolution - 1 - depth;
		size_t index = localX * resolution + sampleY;
		float t = 1.0f - static_cast<float>(depth) / blendingRadius;
		float strength = t * t * (3.0f - 2.0f * t);
		float expected = baseChunk[index] +
			strength * (sharedHeight - baseChunk[index]);

		EXPECT_NEAR(blendedChunk[index], expected, 1e-5f);
	}

	size_t outsideRadiusIndex =
		(resolution - 1 - blendingRadius) * resolution + sampleY;
	EXPECT_FLOAT_EQ(blendedChunk[outsideRadiusIndex], baseChunk[outsideRadiusIndex]);

	destroyContext(ctx);
}

TEST(BlenderTests, BlenderSmoothsWithEdgeSamplingRadiusOne) {
	expectBlenderSmoothsWithEdgeSamplingRadius(1);
}

TEST(BlenderTests, BlenderSmoothsWithEdgeSamplingRadiusThree) {
	expectBlenderSmoothsWithEdgeSamplingRadius(3);
}

TEST(BlenderTests, BlenderGetPointMatchesChunkWithinBlendingRadius) {
	int resolution = 16;
	int blendingRadius = 3;
	int sampleY = resolution / 2;
	Context ctx = createContext(0);
	ASSERT_NE(ctx, nullptr);

	CommonSettings settings{ 123, 1.0f, 1.0f, resolution, true };
	Generator base = createPerlinGenerator(ctx, settings);
	ASSERT_NE(base, nullptr);
	Generator blender = createBlenderGenerator(ctx, settings, BlenderSettings{ blendingRadius, 1, base });
	ASSERT_NE(blender, nullptr);

	std::vector<float> chunk(resolution * resolution);
	getChunk(blender, 0, 0, chunk.data());
	for (int depth = 0; depth < blendingRadius; ++depth) {
		int x = resolution - 1 - depth;
		size_t index = x * resolution + sampleY;
		EXPECT_FLOAT_EQ(getPoint(blender, x, sampleY), chunk[index]);
	}
	int outsideRadiusX = resolution - 1 - blendingRadius;
	EXPECT_FLOAT_EQ(
		getPoint(blender, outsideRadiusX, sampleY),
		chunk[outsideRadiusX * resolution + sampleY]);

	destroyContext(ctx);
}

int main(int argc, char** argv) { 
	::testing::InitGoogleTest(&argc, argv);
	int result = RUN_ALL_TESTS();
	return result;
}
