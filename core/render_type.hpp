#pragma once
#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace lc {
	typedef glm::dvec4 Vec4;
	typedef glm::dvec3 Vec3;
	typedef glm::dvec2 Vec2;
	typedef glm::dmat4 Mat4;
	typedef glm::dmat3 Mat3;
	typedef glm::dquat Quat;

	/*
	光線
	常にdは正規化されていると約束する
	*/
	struct Ray {
		Vec3 o;
		Vec3 d;

		Ray() {}
		Ray(Vec3 origin, Vec3 direction) :o(origin), d(direction) {}
	};
}