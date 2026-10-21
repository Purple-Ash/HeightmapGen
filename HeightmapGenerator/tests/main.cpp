#include "HeightmapGenAPI.h"
#include "../library/core/Helpers.h"
#include <gtest/gtest.h>
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

TEST(OrtModelTests, BPModelInvalidPathTest) {
    Context ctx = createContext(0);
    CommonSettings commonSettings{42, 1.0f, 1.0f, 3, true};

    const char* invalidPaths[] = {
        nullptr,
        "missing.onnx",
    };
    for (const char* modelPath : invalidPaths) {
        Generator gen = nullptr;
        EXPECT_NO_THROW(gen = createBPGenerator(ctx, commonSettings, modelPath));
        EXPECT_EQ(gen, nullptr);
        destroyGenerator(gen);
    }

    // Failed loads must leave the context usable for subsequent creation.
    Generator gen = createBPGenerator(ctx, commonSettings, "identity.onnx");
    EXPECT_NE(gen, nullptr);
    destroyContext(ctx);
}

TEST(OrtModelTests, BPModelSmokeTest) {
    Context ctx = createContext(0);
    CommonSettings commonSettings{};
    commonSettings.resolution = 3;
    commonSettings.scale = 1.0f;
    commonSettings.cacheable = true;
    
    const char* modelPath = "identity.onnx";

    Generator gen = createBPGenerator(ctx, commonSettings, modelPath);
    ASSERT_NE(gen, nullptr);

    std::vector<float> chunkData(commonSettings.resolution * commonSettings.resolution);
    getChunk(gen, 1, 1, chunkData.data());

    float pointSample = getPoint(gen, 10, 10);
    (void)pointSample;

    destroyGenerator(gen);
    destroyContext(ctx);
}

TEST(OrtModelTests, BPModelSeedDeterminism) {
    Context ctx = createContext(0);
    CommonSettings settings{42, 1.0f, 1.0f, 3, false};
    Generator first = createBPGenerator(ctx, settings, "identity.onnx");
    Generator second = createBPGenerator(ctx, settings, "identity.onnx");
    settings.seed += uint64_t{1} << 32;
    Generator differentSeed = createBPGenerator(ctx, settings, "identity.onnx");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(differentSeed, nullptr);

    std::vector<float> expected(9), actual(9), otherChunk(9);
    getChunk(first, -2, 3, expected.data());
    getChunk(first, 3, -2, otherChunk.data());
    EXPECT_NE(expected, otherChunk);
    getChunk(first, -2, 3, actual.data());
    EXPECT_EQ(expected, actual);
    getChunk(second, 3, -2, actual.data());
    EXPECT_EQ(otherChunk, actual);
    getChunk(second, -2, 3, actual.data());
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(getPoint(first, -3, 7), expected[4]);
    getChunk(differentSeed, -2, 3, actual.data());
    EXPECT_NE(expected, actual);

    settings.seed = 42;
    settings.cacheable = true;
    Generator cached = createBPGenerator(ctx, settings, "identity.onnx");
    ASSERT_NE(cached, nullptr);
    getChunk(cached, -2, 3, actual.data());
    EXPECT_EQ(expected, actual);
    cleanChunkFromCache(cached, -2, 3);
    getChunk(cached, -2, 3, actual.data());
    EXPECT_EQ(expected, actual);
    destroyContext(ctx);
}

TEST(OrtModelTests, BPModelSizeMismatchTest) {
    Context ctx = createContext(0);
    CommonSettings commonSettings{};
    commonSettings.resolution = 10;
    commonSettings.scale = 1.0f;
    commonSettings.cacheable = true;
    
    const char* modelPath = "identity.onnx";
    Generator gen = createBPGenerator(ctx, commonSettings, modelPath);
    ASSERT_EQ(gen, nullptr);
    destroyContext(ctx);
}

TEST(OrtModelTests, BPModelThreadedOutputMatchesSerial) {
    Context ctx = createContext(4);
    CommonSettings settings{42, 1.0f, 1.0f, 3, false};
    Generator serial = createBPGenerator(ctx, settings, "identity.onnx");
    settings.cacheable = true;
    Generator threaded = createBPGenerator(ctx, settings, "identity.onnx");
    ASSERT_NE(serial, nullptr);
    ASSERT_NE(threaded, nullptr);

    constexpr int chunkCount = 64;
    std::vector<std::vector<float>> expected(chunkCount, std::vector<float>(9));
    for (int i = 0; i < chunkCount; i++) {
        getChunk(serial, i - 32, 3 - i, expected[i].data());
    }
    for (int i = 0; i < chunkCount; i++) {
        requestChunk(threaded, i - 32, 3 - i);
    }
    for (int i = 0; i < chunkCount; i++) {
        std::vector<float> actual(9);
        getChunk(threaded, i - 32, 3 - i, actual.data());
        EXPECT_EQ(expected[i], actual) << "Chunk " << i;
    }

    // Removal must also safely drain queued inference jobs.
    for (int i = 0; i < chunkCount; i++) {
        requestChunk(threaded, i + 64, i);
    }
    destroyGenerator(threaded);
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

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	int result = RUN_ALL_TESTS();
	return result;
}
