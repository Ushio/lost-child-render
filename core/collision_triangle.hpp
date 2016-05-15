#pragma once

#include <array>
#include "render_type.hpp"
#include "collision.hpp"
#include <boost/optional.hpp>

/*
  �Ԃꂽ�O�p�`�Ȃǂ͍l���Ȃ����Ƃɂ���
*/
namespace lc {
	struct Triangle {
		Triangle() {}
		Triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2) :v{v0, v1, v2} {

		}
		std::array<Vec3, 3> v;
	};

	inline Vec3 triangle_normal(const Triangle &triangle, bool isback) {
		Vec3 e1 = triangle.v[1] - triangle.v[0];
		Vec3 e2 = triangle.v[2] - triangle.v[0];
		return glm::normalize(isback ? glm::cross(e2, e1) : glm::cross(e1, e2));
	}
	struct TriangleIntersection : public Intersection {
		Vec3 intersect_normal(const Triangle &triangle) const {
			return triangle_normal(triangle, isback);
		}
		bool isback = false;
		Vec2 uv;
	};
	inline boost::optional<TriangleIntersection> intersect(const Ray &ray, const Triangle &triangle) {
		Vec3 v0 = triangle.v[0];
		Vec3 v1 = triangle.v[1];
		Vec3 v2 = triangle.v[2];

		Vec3 baryPosition;

		Vec3 e1 = v1 - v0;
		Vec3 e2 = v2 - v0;

		Vec3 p = glm::cross(ray.d, e2);

		double a = glm::dot(e1, p);

		double f = 1.0 / a;

		Vec3 s = ray.o - v0;
		baryPosition.x = f * glm::dot(s, p);
		if (baryPosition.x < 0.0)
			return boost::none;
		if (baryPosition.x > 1.0)
			return boost::none;

		Vec3 q = glm::cross(s, e1);
		baryPosition.y = f * glm::dot(ray.d, q);
		if (baryPosition.y < 0.0)
			return boost::none;
		if (baryPosition.y + baryPosition.x > 1.0)
			return boost::none;

		baryPosition.z = f * glm::dot(e2, q);

		bool isIntersect = baryPosition.z >= 0.0;
		if (isIntersect == false) {
			return boost::none;
		}

		bool isback = a < 0.0;

		TriangleIntersection intersection;
		intersection.tmin = baryPosition.z;
		intersection.isback = isback;
		intersection.uv = Vec2(baryPosition.x, baryPosition.y);
		return intersection;
	}
}