#pragma once

#include <boost/variant.hpp>
#include "render_type.hpp"

namespace lc {
	struct LambertMaterial {
		LambertMaterial() {}
		LambertMaterial(Vec3 albedo_) :albedo(albedo_) {}

		Vec3 albedo = Vec3(1.0);
	};
	struct CookTorranceMaterial {
		CookTorranceMaterial() {}
		CookTorranceMaterial(Vec3 albedo_, double roughness_, double fesnel_coef_)
			:albedo_diffuse(albedo_), albedo_specular(albedo_), roughness(roughness_), fesnel_coef(fesnel_coef_) {}
		CookTorranceMaterial(Vec3 albedo_diffuse_, Vec3 albedo_specular_, double roughness_, double fesnel_coef_)
			:albedo_diffuse(albedo_diffuse_), albedo_specular(albedo_specular_), roughness(roughness_), fesnel_coef(fesnel_coef_) {}

		Vec3 albedo_diffuse = Vec3(1.0);
		Vec3 albedo_specular = Vec3(1.0);
		double roughness = 0.5;
		double fesnel_coef = 0.98;
	};
	struct EmissiveMaterial {
		EmissiveMaterial() {}
		EmissiveMaterial(Vec3 color_) :color(color_) {}

		Vec3 color = Vec3(1.0);
	};
	struct RefractionMaterial {
		RefractionMaterial() {}
		RefractionMaterial(double ior_) :ior(ior_) {}
		double ior = 1.4;
	};
	struct PerfectSpecularMaterial {
		PerfectSpecularMaterial() {}
	};

	typedef boost::variant<EmissiveMaterial, LambertMaterial, CookTorranceMaterial, RefractionMaterial, PerfectSpecularMaterial> Material;

	struct MicroSurface {
		Vec3 p;
		Vec3 n;
		Vec3 vn; /* virtual normal */
		bool isback = false;
		Material m;
	};
}