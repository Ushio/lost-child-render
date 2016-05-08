#pragma once
#include <glm/glm.hpp>

namespace lc {
	typedef glm::dvec3 Vec3;
	typedef glm::dvec2 Vec2;

	struct Ray {
		Vec3 o;
		Vec3 d;
	};


}