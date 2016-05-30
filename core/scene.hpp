#pragma once

#include <boost/variant.hpp>

#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "material.hpp"

namespace lc {
	template <class F>
	Vec3 marching_normal(const Vec3 &p, F f) {
		double d = 0.001;
		return glm::normalize(Vec3(
			f(p + Vec3(d, 0.0, 0.0)) - f(p + Vec3(-d, 0.0, 0.0)),
			f(p + Vec3(0.0, d, 0.0)) - f(p + Vec3(0.0, -d, 0.0)),
			f(p + Vec3(0.0, 0.0, d)) - f(p + Vec3(0.0, 0.0, -d))
		));
	}


	struct SphereObject {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}
		Sphere sphere;
		Material material;
		bool use_importance = true;
	};

	struct TriangleMeshObject {
		std::string mesh;

		/* reconstruct member */
		BVH bvh;
	};

	struct ConelBoxObject {
		ConelBoxObject() :ConelBoxObject(5.0) {}
		ConelBoxObject(double size) {
			double hsize = size * 0.5;
			Vec3 R(0.75, 0.25, 0.25);
			Vec3 G(0.25, 0.25, 0.75);
			Vec3 W(0.75);

			Vec3 ps[] = {
				Vec3(-hsize, hsize, -hsize),
				Vec3(-hsize, hsize, hsize),
				Vec3(hsize, hsize, hsize),
				Vec3(hsize, hsize, -hsize),

				Vec3(-hsize, -hsize, -hsize),
				Vec3(-hsize, -hsize, hsize),
				Vec3(hsize, -hsize, hsize),
				Vec3(hsize, -hsize, -hsize)
			};

			triangles.emplace_back(Triangle(ps[0], ps[1], ps[4]), R);
			triangles.emplace_back(Triangle(ps[1], ps[5], ps[4]), R);

			triangles.emplace_back(Triangle(ps[2], ps[3], ps[7]), G);
			triangles.emplace_back(Triangle(ps[6], ps[2], ps[7]), G);

			triangles.emplace_back(Triangle(ps[0], ps[1], ps[2]), W);
			triangles.emplace_back(Triangle(ps[0], ps[2], ps[3]), W);

			triangles.emplace_back(Triangle(ps[0], ps[4], ps[7]), W);
			triangles.emplace_back(Triangle(ps[3], ps[0], ps[7]), W);

			triangles.emplace_back(Triangle(ps[4], ps[5], ps[6]), W);
			triangles.emplace_back(Triangle(ps[6], ps[7], ps[4]), W);
		}

		struct ColorTriangle {
			ColorTriangle() { }
			ColorTriangle(const Triangle &t, const Vec3 &c) :triangle(t), color(c) {
			}
			Triangle triangle;
			Vec3 color;
		};
		std::vector<ColorTriangle> triangles;
	};
	struct ConelBoxIntersection : public Intersection {
		Vec3 intersect_normal() const {
			return triangle_normal(triangle.triangle, isback);
		}
		ConelBoxObject::ColorTriangle triangle;
		bool isback = false;
	};
	inline boost::optional<ConelBoxIntersection> intersect(const Ray &ray, const ConelBoxObject &b) {
		boost::optional<ConelBoxIntersection> r;
		for (int i = 0; i < b.triangles.size(); ++i) {
			if (auto intersection = intersect(ray, b.triangles[i].triangle)) {
				double tmin = r ? r->tmin : std::numeric_limits<double>::max();
				if (intersection->tmin < tmin) {
					ConelBoxIntersection newIntersection;
					newIntersection.tmin = intersection->tmin;
					newIntersection.isback = intersection->isback;
					newIntersection.triangle = b.triangles[i];
					r = newIntersection;
				}
			}
		}
		return r;
	}


	typedef boost::variant<SphereObject, ConelBoxObject> SceneObject;

	struct SphereObjectIntersection {
		const SphereObject *o = nullptr;
		SphereIntersection intersection;
	};
	struct ConelBoxObjectIntersection {
		const ConelBoxObject *o = nullptr;
		ConelBoxIntersection intersection;
	};
	typedef boost::variant<SphereObjectIntersection, ConelBoxObjectIntersection> ObjectIntersection;

	struct MicroSurfaceVisitor : public boost::static_visitor<MicroSurface> {
		MicroSurfaceVisitor(const Ray &r) :ray(r) {}
		MicroSurface operator()(const SphereObjectIntersection &intersection) {
			MicroSurface m;
			m.p = intersection.intersection.intersect_position(ray);
			m.n = intersection.intersection.intersect_normal(intersection.o->sphere.center, m.p);
			m.vn = m.n;
			m.m = intersection.o->material;
			m.isback = intersection.intersection.isback;
			return m;
		}
		MicroSurface operator()(const ConelBoxObjectIntersection &intersection) {
			MicroSurface m;
			m.p = intersection.intersection.intersect_position(ray);
			m.n = intersection.intersection.intersect_normal();
			m.vn = m.n;
			m.m = LambertMaterial(intersection.intersection.triangle.color);
			m.isback = intersection.intersection.isback;
			return m;
		}
		Ray ray;
	};

	struct ImportantArea {
		ImportantArea() {}
		ImportantArea(const Sphere &sphere) :shape(sphere) {}
		// ImportantArea(const AABB &aabb) :shape(aabb) {}

		Sphere shape;

		// boost::variant<Sphere, AABB> shape;

		/* p が重点サンプルの基準点 */
		template <class E>
		Sample<Vec3> sample(const Vec3 &p, RandomEngine<E> &engine) const {
			const double eps = 0.001;
			const double pdf_min = 0.05;

			double distance_pq;
			Vec3 q;
			Vec3 qn;
			do {
				q = shape.center + generate_on_sphere(engine) * shape.radius;
				qn = glm::normalize(q - shape.center);
				distance_pq = glm::distance(p, q);
			} while (distance_pq < eps);

			Sample<Vec3> s;
			s.value = q;
			double A = 4.0 * glm::pi<double>() * shape.radius * shape.radius;
			double cos_alpha = glm::abs(glm::dot(qn, (p - q) / distance_pq));
			s.pdf = distance_pq * distance_pq / (A * cos_alpha);
			s.pdf = glm::max(s.pdf, pdf_min);
			return s;
		}
	};

	struct Scene {
		Transform viewTransform;
		Camera camera;
		std::vector<SceneObject> objects;
		std::vector<ImportantArea> importances;
	};

	template <class E>
	Sample<Vec3> sample_important_position(const Scene &scene, const Vec3 &p, RandomEngine<E> &engine) {
		return scene.importances[engine() % scene.importances.size()].sample(p, engine);
	}

	inline boost::optional<MicroSurface> intersect(const Ray &ray, const Scene &scene) {
		double tmin = std::numeric_limits<double>::max();
		ObjectIntersection min_intersection;

		for (int i = 0; i < scene.objects.size(); ++i) {
			if (auto *s = boost::get<SphereObject>(&scene.objects[i])) {
				if (auto intersection = intersect(ray, s->sphere)) {
					if (intersection->tmin < tmin) {
						SphereObjectIntersection soi;
						soi.o = s;
						soi.intersection = *intersection;

						tmin = intersection->tmin;
						min_intersection = soi;
					}
				}
			}
			if (auto *c = boost::get<ConelBoxObject>(&scene.objects[i])) {
				if (auto intersection = intersect(ray, *c)) {
					if (intersection->tmin < tmin) {
						ConelBoxObjectIntersection soi;
						soi.o = c;
						soi.intersection = *intersection;

						tmin = intersection->tmin;
						min_intersection = soi;
					}
				}
			}
		}

		//// テストレイマーチング
		//Vec3 o = ray.o;
		//double r_min = 0.0;
		//bool hit = false;
		//auto udRoundBox = [](const Vec3 & p, const Vec3 & b, double r) {
		//	return glm::length(glm::max(glm::abs(p) - b, 0.0)) - r;
		//};
		//auto sdSphere = [](const Vec3 &  p, double s) {
		//	return glm::length(p) - s;
		//};
		////auto smin = [](double a, double b, double k) {
		////	double res = glm::exp(-k*a) + exp(-k*b);
		////	return -glm::log(res) / k;
		////};
		//auto smin = [](double a, double b, double k)
		//{
		//	double h = glm::clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
		//	return glm::mix(b, a, h) - k * h* (1.0 - h);
		//};
		//auto distance_function = [=](Vec3 p) {
		//	p += Vec3(5.0, 5.0, 0.0);
		//	double a = sdSphere(p + Vec3(-8.0, 0.0, 0.0), 5.0);
		//	double b = sdSphere(p, 5.0);
		//	double c = sdSphere(p + Vec3(-3.0, 5.0, -4.0), 5.0);
		//	double d = sdSphere(p + Vec3(6.0, 1.0, -3.0), 5.0);
		//	double k = 2.0;
		//	return smin(smin(a, b, k), smin(c, d, k), k);
		//	// return udRoundBox(p, Vec3(5.0, 5.0, 5.0), 2.0);
		//};
		//double d;
		//for (int i = 0; i < 100; ++i) {
		//	d = distance_function(ray.o + ray.d * r_min);
		//	if (1000.0 < r_min) {
		//		break;
		//	}
		//	r_min += d;
		//}
		//if (d < 0.001) {
		//	hit = true;
		//}
		//if (hit && r_min < tmin) {
		//	MicroSurface m;
		//	m.p = ray.o + ray.d * r_min;
		//	m.n = marching_normal(m.p, distance_function);
		//	m.vn = m.n;
		//	m.isback = false;
		//	// m.m = LambertMaterial(Vec3(1.0));
		//	m.m = PerfectSpecularMaterial();
		//	if (glm::any(glm::isfinite(m.n))) {
		//		return m;
		//	}
		//}

		if (tmin != std::numeric_limits<double>::max()) {
			return min_intersection.apply_visitor(MicroSurfaceVisitor(ray));
		}
		return boost::none;
	}
}