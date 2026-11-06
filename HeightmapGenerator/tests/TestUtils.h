#pragma once

#include "HeightmapGenAPI.h"
#include <memory>

class CountingChunkGenerator {
	struct Impl;
	std::unique_ptr<Impl> impl;

public:
	CountingChunkGenerator(Context ctx, const CommonSettings& settings);
	~CountingChunkGenerator();

	CountingChunkGenerator(const CountingChunkGenerator&) = delete;
	CountingChunkGenerator& operator=(const CountingChunkGenerator&) = delete;

	Generator get() const;
	int generatedChunkCount() const;
	void destroy();
};
