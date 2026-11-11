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


enum class Edge : uint8_t {
    North = 1u << 0,
    East  = 1u << 1,
    South = 1u << 2,
    West  = 1u << 3
};

enum class Corner : uint8_t {
    NorthEast = 1u << 4,
    SouthEast = 1u << 5,
    SouthWest = 1u << 6,
    NorthWest = 1u << 7
};

inline int32_t chunkCoordsToChunkIdx(Vec2Int point, int32_t resolution) {
    return point.x * resolution + point.y;
}
