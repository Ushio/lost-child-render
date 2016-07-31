#pragma once

#include "render_type.hpp"

namespace lc {
	// ゼロ割はここで気を付けたほうがいいのだろうか？

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

	// フレネル (Schlick近似)
	inline double fresnel(double costheta, double f0) {
		return f0 + (1.0 - f0) * glm::pow(1.0 - costheta, 5.0);
	}

	// 
	inline double Gp(const Vec3 &v, const Vec3 &h, const Vec3 &n, double roughness) {
		double VoH = glm::clamp(glm::dot(v, h), 0.0, 1.0);
		double VoH2 = VoH * VoH;
		double X = VoH / glm::clamp(glm::dot(v, n), 0.0, 1.0) < 0.0 ? 0.0 : 1.0;
		double div = 1.0 + glm::sqrt(1.0 + glm::pow(roughness, 4) * (1.0 - VoH2) / VoH2);
		return X * 2.0f / div;
	}
	inline double G(const Vec3 &omega_i, const Vec3 &omega_o, const Vec3 &h, const Vec3 &n, double roughness) {
		return Gp(omega_i, h, n, roughness) * Gp(omega_o, h, n, roughness);
	}
}
