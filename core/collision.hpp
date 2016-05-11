#pragma once

#include "render_type.hpp"

namespace lc {
	struct Intersection {
		double tmin = 0.0;
		Vec3 p;
		Vec3 n;
		bool isback = false;
	};
}