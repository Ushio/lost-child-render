#pragma once

#include <boost/variant.hpp>

#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "material.hpp"

namespace lc {
	struct SphereObject {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}
		Sphere sphere;
		Material material;
		// bool use_importance = true;
	};

	struct TriangleMeshObject {
		BVH bvh;
		Material material;
		Transform transform;
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


	typedef boost::variant<SphereObject, ConelBoxObject, TriangleMeshObject> SceneObject;

	struct SphereObjectIntersection {
		const SphereObject *o = nullptr;
		SphereIntersection intersection;
	};
	struct ConelBoxObjectIntersection {
		const ConelBoxObject *o = nullptr;
		ConelBoxIntersection intersection;
	};
	struct TriangleMeshObjectIntersection {
		const TriangleMeshObject *o = nullptr;
		BVH::BVHIntersection intersection;
	};
	typedef boost::variant<SphereObjectIntersection, ConelBoxObjectIntersection, TriangleMeshObjectIntersection> ObjectIntersection;

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
		MicroSurface operator()(const TriangleMeshObjectIntersection &intersection) {
			MicroSurface m;
			m.p = intersection.intersection.intersect_position(intersection.o->transform.to_local_ray(ray));
			m.n = intersection.intersection.intersect_normal(intersection.o->bvh._triangles[intersection.intersection.triangle_index]);
			m.vn = m.n;
			m.m = intersection.o->material;
			m.isback = intersection.intersection.isback;
			
			m.p = intersection.o->transform.from_local_position(m.p);
			m.n = intersection.o->transform.from_local_normal(m.n);
			m.vn = intersection.o->transform.from_local_normal(m.vn);

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
			if (auto *c = boost::get<TriangleMeshObject>(&scene.objects[i])) {
				if (auto intersection = c->bvh.intersect(c->transform.to_local_ray(ray))) {
					if (intersection->tmin < tmin) {
						TriangleMeshObjectIntersection tmoi;
						tmoi.intersection = *intersection;
						tmoi.o = c;

						tmin = intersection->tmin;
						min_intersection = tmoi;
					}
				}
			}
		}

		if (tmin != std::numeric_limits<double>::max()) {
			return min_intersection.apply_visitor(MicroSurfaceVisitor(ray));
		}
		return boost::none;
	}
}