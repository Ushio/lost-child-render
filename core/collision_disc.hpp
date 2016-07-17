#pragma once

#include "collision_plane.hpp"

namespace lc {
	struct Disc {
		Plane plane;
		Vec3 origin;
		double radius = 1.0;
	};

	/*
	p: 平面上の点
	n: 法線, 正規化されていなければならない
	*/
	inline Disc make_disc(Vec3 p, Vec3 n, double radius) {
		Disc disc;
		disc.plane = make_plane_pn(p, n);
		disc.origin = p;
		disc.radius = radius;
		return disc;
	}

	inline boost::optional<PlaneIntersection> intersect(const Ray &ray, const Disc &disc) {
		if (auto intersection = intersect(ray, disc.plane)) {
			auto intersect_p = intersection->intersect_position(ray);
			if (glm::distance2(intersect_p, disc.origin) < disc.radius * disc.radius) {
				return intersection;
			}
		}
		return boost::none;
	}
}
