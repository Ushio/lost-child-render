#pragma once

#include "render_type.hpp"

namespace lc {
	// cos_thetaは、面法線と、マイクロファセット法線の角度
	inline double ggx_d(double cos_theta, double roughness) {
		double a = roughness * roughness;
		double aa = a * a;
		double term1 = aa;
		double term2 = (aa - 1.0) * glm::pow(cos_theta, 2) + 1.0;
		return term1 / (glm::pi<double>() * term2 * term2);
	}
	// 立体角のPDFはさらにcos_thetaが必要らしい
	inline double ggx_pdf(double cos_theta, double roughness) {
		return ggx_d(cos_theta, roughness) * cos_theta;
	}
}
