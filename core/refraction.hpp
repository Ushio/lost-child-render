#pragma once

#include "render_type.hpp"
namespace lc {
	/*
	全反射を考慮に入れた屈折計算
	*/
	inline Vec3 refraction(const Vec3 &I, const Vec3 &N, double eta) {
		double NoI = glm::dot(N, I);
		double k = 1.0 - eta * eta * (1.0 - NoI * NoI);
		if (k <= 0.0) {
			return I - 2.0 * N * NoI;
		}
		return eta * I - (eta * NoI + glm::sqrt(k)) * N;
	}
}
