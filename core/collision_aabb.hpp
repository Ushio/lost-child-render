#pragma once

#include "render_type.hpp"
#include "collision.hpp"
#include <boost/optional.hpp>

namespace lc {
	struct AABB {
		Vec3 min_position = Vec3(std::numeric_limits<double>::max());
		Vec3 max_position = Vec3(-std::numeric_limits<double>::max());
		AABB() {}
		AABB(const Vec3 &min_p, const Vec3 &max_p) :min_position(min_p), max_position(max_p) {}
	};

	inline AABB expand(const AABB &aabb, const Vec3 &p) {
		return AABB(
			glm::min(aabb.min_position, p),
			glm::max(aabb.max_position, p)
		);
	}
	inline bool contains(const AABB &aabb, const Vec3 &p)  {
		for (int dim = 0; dim < 3; ++dim) {
			if (p[dim] < aabb.min_position[dim] || aabb.max_position[dim] < p[dim]) {
				return false;
			}
		}
		return true;
	}
	inline bool empty(const AABB &aabb) {
		return glm::any(glm::greaterThan(aabb.min_position, aabb.max_position));
	}
	inline boost::optional<Intersection> intersect(const Ray &ray, const AABB &aabb) {
		auto p = ray.o;
		auto d = ray.d;

		// set to -FLT_MAX to get first hit on line
		double tmin = 0.0;

		// set to max distance ray can travel (for segment)
		double tmax = std::numeric_limits<double>::max();

		for (int i = 0; i < 3; i++) {
			if (glm::abs(d[i]) < glm::epsilon<double>()) {
				// 光線はスラブに対して平行。原点がスラブの中になければ交差なし
				if (p[i] < aabb.min_position[i] || p[i] > aabb.max_position[i]) return boost::none;
			}
			else {
				// スラブの近い平面および遠い平面と交差する光線のtの値を計算
				double ood = 1.0 / d[i];
				double t1 = (aabb.min_position[i] - p[i]) * ood;
				double t2 = (aabb.max_position[i] - p[i]) * ood;
				// t1が近い平面との交差、t2が遠い平面との交差となる
				if (t1 > t2) std::swap(t1, t2);
				// スラブの交差している間隔との交差を計算
				if (t1 > tmin) tmin = t1;
				if (t2 < tmax) tmax = t2;
				// スラブに交差がないことが分かれば衝突はないのですぐに終了
				if (tmin > tmax) return boost::none;
			}
		}
		Intersection intersection;
		intersection.tmin = tmin;
		return intersection;
	}
}