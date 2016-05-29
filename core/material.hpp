#pragma once

#include <boost/variant.hpp>
#include "render_type.hpp"

namespace lc {
	struct LambertMaterial {
		LambertMaterial() {}
		LambertMaterial(Vec3 albedo_) :albedo(albedo_) {}

		Vec3 albedo = Vec3(1.0);
	};
	struct EmissiveMaterial {
		EmissiveMaterial() {}
		EmissiveMaterial(Vec3 color_) :color(color_) {}

		Vec3 color = Vec3(1.0);
	};

	typedef boost::variant<EmissiveMaterial, LambertMaterial> Material;

	struct MicroSurface {
		Vec3 p;
		Vec3 n;
		Vec3 vn; /* virtual normal */
		bool isback = false;
		Material m;
	};
}