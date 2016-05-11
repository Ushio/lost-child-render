#pragma once

#include "render_type.hpp"
#include "collision.hpp"
#include <boost/optional.hpp>

namespace lc {
	/*
	d が正の時は、原点は平面の表側(平面から見て法線ベクトルの方向)にあります。
	d が負の時は、原点は平面の裏側(法線ベクトルと逆の方向)にあります。
	n = 平面の法線。平面上の点xに対してDot(n,x) = dが成立
	d = 平面上のある与えられた点pに対してd = Dot(n,p)が成立
	*/
	struct Plane {
		Vec3 n = Vec3(0.0, 1.0, 0.0);
		double d = 0.0;
	};

	/*
	p: 平面上の点
	n: 法線, 正規化されていなければならない
	*/
	inline Plane make_plane_pn(Vec3 p, Vec3 n) {
		Plane plane;
		plane.n = n;
		plane.d = dot(n, p);
		return plane;
	}

	inline boost::optional<Intersection> intersect(const Ray &ray, const Plane &p) {
		double DoN = glm::dot(ray.d, p.n);
		if (glm::abs(DoN) <= glm::epsilon<double>()) {
			return boost::none;
		}
		double tmin = (p.d - dot(p.n, ray.o)) / DoN;
		if (tmin <= 0.0) {
			return boost::none;
		}
		Intersection intersection;
		intersection.tmin = tmin;
		intersection.isback = 0.0 < DoN;
		intersection.n = intersection.isback ? -p.n : p.n;
		intersection.p = ray.o + ray.d * tmin;
		return intersection;
	}
}
