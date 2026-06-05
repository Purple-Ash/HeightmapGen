#pragma once
#include <cmath>

struct Vec3 {

	float x;
	float y;
	float z;

	float distance(const Vec3& other) const {
		float dx = x - other.x;
		float dy = y - other.y;
		float dz = z - other.z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

};

struct Vec2 {

	float x;
	float y;

};