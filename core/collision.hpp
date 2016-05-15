#pragma once

#include "render_type.hpp"

namespace lc {
	struct Intersection {
		double tmin = 0.0;
		Vec3 intersect_position(const Ray &ray) const {
			return ray.o + ray.d * tmin;
		}
	};
}

