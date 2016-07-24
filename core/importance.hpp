#pragma once

#include <tuple>

#include "render_type.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "brdf.hpp"

// 値と確率密度のセット。汎用構造体
namespace lc {
	template <class T>
	struct Sample {
		T value;
		double pdf = 0.0;
	};
}

// BRDFに基づいた重点サンプリング
namespace lc{
	// eps は 0-1
	Sample<Vec3> importance_lambert(const std::tuple<double, double> &eps, const Vec3 &n) {
		double eps_1 = std::get<0>(eps);
		double eps_2 = std::get<1>(eps);

		double theta = glm::acos(glm::sqrt(1.0 - eps_1));
		double phi = glm::two_pi<double>() * eps_2;
		double cos_theta = glm::cos(theta);

		// R = 1
		// -x <-> +x
		//  0 <-> +y
		// -z <-> +z
		double sin_theta = glm::sin(theta);
		double z = sin_theta * glm::cos(phi);
		double x = sin_theta * glm::sin(phi);
		double y = cos_theta;

		HemisphereTransform hemisphereTransform(n);

		Sample<Vec3> sample;
		sample.value = hemisphereTransform.transform(Vec3(x, y, z));
		sample.pdf = glm::max(cos_theta * glm::one_over_pi<double>(), 0.0001);

		return sample;
	}

	// eps は 0-1
	Sample<Vec3> importance_ggx(const std::tuple<double, double> &eps, const Vec3 &n, const Vec3 &omega_o, double roughness) {
		double a = roughness * roughness;
		double aa = a * a;

		double eps_1 = std::get<0>(eps);
		double eps_2 = std::get<1>(eps);
		double phi = glm::two_pi<double>() * eps_1;

		double theta = glm::acos(
			glm::sqrt(
			(1.0 - eps_2) / (eps_2 * (aa - 1.0) + 1.0)
			)
		);

		// R = 1
		// -x <-> +x
		//  0 <-> +y
		// -z <-> +z
		double cos_theta = glm::cos(theta);
		double sin_theta = glm::sin(theta);
		double z = sin_theta * glm::cos(phi);
		double x = sin_theta * glm::sin(phi);
		double y = cos_theta;

		Vec3 h = Vec3(x, y, z);
		auto omega_i = glm::reflect(-omega_o, h);

		Sample<Vec3> sample;
		sample.value = omega_i;

		double pdf_h = ggx_pdf(cos_theta, roughness);
		double pdf = pdf_h / (4.0 * glm::dot(omega_o, h));

		sample.pdf = pdf;

		return sample;
	}
}