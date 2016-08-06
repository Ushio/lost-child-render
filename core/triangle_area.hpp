#pragma once

#include "render_type.hpp"

namespace lc {
	// 三角形の面積を求める
	inline double triangle_area(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2) {
		auto va = p0 - p1;
		auto vb = p2 - p1;
		return glm::length(glm::cross(va, vb)) * 0.5;
	}
}