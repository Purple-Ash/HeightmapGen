#pragma once
#include <random>
#include <vector>
#include <cstdint>
#include <cmath>

inline float randomFloatBetween(float min, float max)
{
	thread_local std::mt19937 gen(std::random_device{}());

	std::uniform_real_distribution<float> dis(min, max);
	return dis(gen);
}

static constexpr float whiteNoise(int x, int y, int z) {
	uint32_t seed = uint32_t(x) * 1087;
	seed ^= 0xE56FAA12;
	seed += uint32_t(y) * 2749;
	seed ^= 0x69628a2d;
	seed += uint32_t(z) * 3433;
	seed ^= 0xa7b2c49a;

	return (float) (seed % 1000) / 1000.0f;
}

template<typename T>
struct Vec2 {
	T x;
	T y;

	constexpr Vec2() = default;
	constexpr Vec2(T x, T y) noexcept : x(x), y(y) {}

	constexpr bool operator==(const Vec2& other) const = default;

	constexpr Vec2 operator+(const Vec2& other) const {
		return Vec2(x + other.x, y + other.y);
	}

	constexpr Vec2 operator-(const Vec2& other) const {
		return Vec2(x - other.x, y - other.y);
	}

	constexpr Vec2 operator*(const Vec2& other) const {
		return Vec2(x * other.x, y * other.y);
	}

	constexpr Vec2 operator/(const Vec2& other) const {
		return Vec2(x / other.x, y / other.y);
	}

	constexpr Vec2 operator+(T scalar) const {
		return Vec2(x + scalar, y + scalar);
	}

	constexpr Vec2 operator-(T scalar) const {
		return Vec2(x - scalar, y - scalar);
	}

	constexpr Vec2 operator*(T scalar) const {
		return Vec2(x * scalar, y * scalar);
	}

	constexpr Vec2 operator/(T scalar) const {
		return Vec2(x / scalar, y / scalar);
	}
};

template<typename T>
struct Vec3 {

	T x;
	T y;
	T z;

	constexpr T distance(const Vec3& other) const {
		const T dx = x - other.x;
		const T dy = y - other.y;
		const T dz = z - other.z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

};


using Vec2Int = Vec2<int32_t>;
using Vec2Float = Vec2<float>;

using Vec3f = Vec3<float>;

namespace std {
	template<> struct hash<Vec2Int> {
		size_t operator()(const Vec2Int& c) const noexcept {
			size_t h1 = hash<int32_t>{}(c.x);
			size_t h2 = hash<int32_t>{}(c.y);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};
}
