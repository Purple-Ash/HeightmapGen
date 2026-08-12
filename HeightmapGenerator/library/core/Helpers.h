#pragma once
#include <random>

inline float randomFloatBetween(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());

	std::uniform_real_distribution<float> dis(min, max);
	return dis(gen);
}

struct Vec2Int {
	int32_t x;
	int32_t y;

	Vec2Int() = default;
	Vec2Int(int32_t x, int32_t y);
	bool operator==(const Vec2Int& other) const = default;
};
