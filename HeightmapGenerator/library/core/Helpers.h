#pragma once
#include <random>
#include <vector>
#include <cstdint>

inline float randomFloatBetween(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());

	std::uniform_real_distribution<float> dis(min, max);
	return dis(gen);
}

template<typename T>
struct Vec2 {
	T x;
	T y;

	Vec2() = default;
	Vec2(T x, T y){
		this->x = x;
		this->y = y;
	}

	bool operator==(const Vec2<T>& other) const = default;

	Vec2<T> operator+(const Vec2<T>& other) const {
		return Vec2<T>(x + other.x, y + other.y);
	}
	Vec2<T> operator-(const Vec2<T>& other) const {
		return Vec2<T>(x - other.x, y - other.y);
	}
	Vec2<T> operator*(const Vec2<T>& other) const {
		return Vec2<T>(x * other.x, y * other.y);
	}
	Vec2<T> operator/(const Vec2<T>& other) const {
		return Vec2<T>(x / other.x, y / other.y);
	}
	Vec2<T> operator+(int32_t scalar) const {
		return Vec2<T>(x + scalar, y + scalar);
	}
	Vec2<T> operator-(int32_t scalar) const {
		return Vec2<T>(x - scalar, y - scalar);
	}
	Vec2<T> operator*(int32_t scalar) const {
		return Vec2<T>(x * scalar, y * scalar);
	}
	Vec2<T> operator/(int32_t scalar) const {
		return Vec2<T>(x / scalar, y / scalar);
	}
};

using Vec2Int = Vec2<int32_t>;
using Vec2Float = Vec2<float>;

namespace std {
	template<> struct hash<Vec2Int> {
		size_t operator()(const Vec2Int& c) const noexcept {
			size_t h1 = hash<int32_t>{}(c.x);
			size_t h2 = hash<int32_t>{}(c.y);
			return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
		}
	};
}
