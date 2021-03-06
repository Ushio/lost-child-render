#pragma once

#include <array>
#include "render_type.hpp"
#include "collision.hpp"
#include <boost/optional.hpp>

/*
  つぶれた三角形などは考えないことにする
*/
namespace lc {
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

	inline boost::optional<TriangleIntersection> intersect(const Ray &ray, const Triangle &triangle, double tmin = std::numeric_limits<double>::max()) {
		const Vec3 &v0 = triangle[0];
		const Vec3 &v1 = triangle[1];
		const Vec3 &v2 = triangle[2];

		Vec3 baryPosition(glm::uninitialize);

		Vec3 e1 = v1 - v0;
		Vec3 e2 = v2 - v0;

		Vec3 p = glm::cross(ray.d, e2);

		double a = glm::dot(e1, p);

		double f = 1.0 / a;

		Vec3 s = ray.o - v0;

		Vec3 q = glm::cross(s, e1);
		baryPosition.z = f * glm::dot(e2, q);
		if (baryPosition.z < 0.0 || tmin < baryPosition.z) {
			return boost::none;
		}

		baryPosition.x = f * glm::dot(s, p);
		if (baryPosition.x < 0.0 || baryPosition.x > 1.0)
			return boost::none;

		baryPosition.y = f * glm::dot(ray.d, q);
		if (baryPosition.y < 0.0 || baryPosition.y + baryPosition.x > 1.0) {
			return boost::none;
		}

		bool isback = a < 0.0;

		TriangleIntersection intersection;
		intersection.tmin = baryPosition.z;
		intersection.isback = isback;
		intersection.uv = Vec2(baryPosition.x, baryPosition.y);
		return intersection;
	}

	inline bool is_visible(const Ray &ray, const Triangle &triangle, double tmin_target) {
		const Vec3 &v0 = triangle[0];
		const Vec3 &v1 = triangle[1];
		const Vec3 &v2 = triangle[2];

		Vec3 baryPosition(glm::uninitialize);

		Vec3 e1 = v1 - v0;
		Vec3 e2 = v2 - v0;

		Vec3 p = glm::cross(ray.d, e2);

		double a = glm::dot(e1, p);

		double f = 1.0 / a;

		Vec3 s = ray.o - v0;

		Vec3 q = glm::cross(s, e1);
		baryPosition.z = f * glm::dot(e2, q);
		if (baryPosition.z < 0.0 || tmin_target < baryPosition.z) {
			return true;
		}

		baryPosition.x = f * glm::dot(s, p);
		if (baryPosition.x < 0.0 || baryPosition.x > 1.0)
			return true;

		baryPosition.y = f * glm::dot(ray.d, q);
		if (baryPosition.y < 0.0 || baryPosition.y + baryPosition.x > 1.0) {
			return true;
		}

		return false;
	}

}