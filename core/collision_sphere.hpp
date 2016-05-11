#pragma once

#include "render_type.hpp"
#include "collision.hpp"
#include <boost/optional.hpp>

namespace lc {
	struct Sphere {
		double radius = 1.0;
		Vec3 center;
	};

	inline boost::optional<Intersection> intersect(const Ray &ray, const Sphere &s) {
		Vec3 m = ray.o - s.center;
		double b = dot(m, ray.d);
		double radius_squared = s.radius * s.radius;
		double c = dot(m, m) - radius_squared;
		if (c > 0.0 && b > 0.0) {
			return boost::none;
		}
		double discr = b * b - c;
		if (discr < 0.0) {
			return boost::none;
		}

		double sqrt_discr = sqrt(discr);
		double tmin_near = -b - sqrt_discr;
		if (0.0 < tmin_near) {
			Intersection intersection;
			intersection.tmin = tmin_near;
			intersection.p = ray.o + ray.d * intersection.tmin;
			intersection.n = normalize(intersection.p - s.center);
			intersection.isback = false;
			return intersection;
		}
		double tmin_far = -b + sqrt_discr;
		if (0.0 < tmin_far) {
			Intersection intersection;
			intersection.tmin = tmin_far;
			intersection.p = ray.o + ray.d * intersection.tmin;
			intersection.n = normalize(s.center - intersection.p);
			intersection.isback = true;
			return intersection;
		}
		return boost::none;
	}
}