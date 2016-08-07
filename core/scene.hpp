﻿#pragma once

#include <boost/variant.hpp>
#include <boost/variant/polymorphic_get.hpp>
#include <boost/range.hpp>
#include <boost/range/join.hpp>

#include "constants.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "collision_plane.hpp"
#include "collision_disc.hpp"
#include "material.hpp"
#include "importance.hpp"
#include "lazy_value.hpp"

namespace lc {
	//struct LightSurface {
	//	Vec3 n;
	//	Vec3 p;
	//};
	//class ILight {
	//public:
	//	virtual ~ILight() {}
	//	virtual EmissiveMaterial emissive_material() const = 0;
	//	virtual Sample<LightSurface> on_light(DefaultEngine &e) const = 0;
	//};

	//struct RectLight : public ILight {
	//	RectLight() {

	//	}
	//	virtual EmissiveMaterial emissive_material() const {
	//		return EmissiveMaterial(color);
	//	}
	//	virtual Sample<LightSurface> on_light(DefaultEngine &e) const {
	//		Sample<LightSurface> ls;
	//		ls.pdf = 1.0 / (size * size);
	//		ls.value.p = Vec3(
	//			generate_continuous(e, -size * 0.5, size * 0.5),
	//			y,
	//			generate_continuous(e, -size * 0.5, size * 0.5));
	//		ls.value.n = Vec3(0.0, -1.0, 0.0);
	//		return ls;
	//	}
	//	double size = 0.0;
	//	double y = 0.0;
	//	Vec3 color;
	//};
	//typedef LazyValue<MicroSurface, 256>

	struct OnLight {
		Vec3 p;
		Vec3 n;
		EmissiveMaterial emissive;
	};
	class ILight {
	public:
		virtual ~ILight() {}
		virtual Sample<OnLight> sample(DefaultEngine &e) const = 0;
		virtual double getArea() const = 0;
		virtual boost::optional<MicroSurface> intersect(const Ray &ray, double &tmin) const = 0;
	};

	struct DiscLight : public ILight {
		DiscLight() {

		}

		Sample<OnLight> sample(DefaultEngine &e) const override {
			HemisphereTransform hemisphereTransform(disc.plane.n);
			auto circle = e.on_circle() * disc.radius;
			auto p = disc.origin + hemisphereTransform.transform(Vec3(circle.x, 0.0, circle.y));

			Sample<OnLight> s;
			s.pdf = 1.0 / this->getArea();
			s.value.p = p;
			s.value.n = disc.plane.n;
			s.value.emissive = emissive;
			return s;
		}
		double getArea() const override {
			return glm::pi<double>() * disc.radius * disc.radius;
		}

		boost::optional<MicroSurface> intersect(const Ray &ray, double &tmin) const override {
			if (auto intersection = lc::intersect(ray, disc)) {
				if (intersection->tmin < tmin) {
					MicroSurface m;
					m.p = intersection->intersect_position(ray);
					m.n = intersection->intersect_normal;
					m.vn = m.n;
					m.m = intersection->isback ? EmissiveMaterial(Vec3(0.0)) : emissive;
					m.isback = intersection->isback;

					tmin = intersection->tmin;
				}
			}
		}

		Disc disc;
		EmissiveMaterial emissive;
		bool doubleSided = false;
	};

	struct PolygonLight {

	};

	struct SphereObject {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}
		Sphere sphere;
		Material material;
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

	typedef boost::variant<SphereObject, ConelBoxObject, TriangleMeshObject, DiscLight> SceneObject;

	struct Scene {
		Transform viewTransform;
		Camera camera;

		void add(SceneObject object) {
			objects.push_back(object);
		}

		void finalize() {
			lights.clear();
			for (size_t i = 0; i < objects.size(); ++i) {
				if (auto *d = boost::get<DiscLight>(&objects[i])) {
					lights.push_back(d);
				}
			}
		}

		std::vector<SceneObject> objects;
		std::vector<ILight *> lights;
	};


	//Sample<OnLight> on_light(const Scene &scene, DefaultEngine &engine) {
	//	const ILight *light = scene.lights.size() == 1 ?
	//		scene.lights[0]
	//		:
	//		scene.lights[engine() % scene.lights.size()];

	//	Sample<OnLight> s = light->sample(engine);
	//	s.pdf /= scene.lights.size();
	//	return s;
	//}

	//struct DirectSample {
	//	Ray ray;
	//	double pdf = 0.0;
	//};

	/*
	直接サンプルするレイを生成
	pdfは立体角尺度
	*/
	inline Sample<Ray> direct_sample_ray(const Scene &scene, const Vec3 &p, DefaultEngine &engine) {
		const ILight *light = scene.lights.size() == 1 ?
			scene.lights[0]
			:
			scene.lights[engine.generate() % scene.lights.size()];

		double selection_pdf = 1.0 / scene.lights.size();

		//double distanceSquared;
		//Vec3 q;
		//Vec3 qn;
		//double area_pdf;
		//do {
		//	Sample<LightSurface> s = light->on_light(engine);
		//	qn = s.value.n;
		//	q = s.value.p;
		//	area_pdf = s.pdf;
		//	distanceSquared = glm::distance2(p, q);
		//} while (distanceSquared < 0.1);

		//Vec3 dir = glm::normalize(q - p);
		//double pdf = distanceSquared * area_pdf / glm::max(glm::dot(qn, -dir), 0.0001);

		// 表面積の確率密度を立体角の確率密度に変換する
		Sample<OnLight> s = light->sample(engine);
		Vec3 dir = glm::normalize(s.value.p - p);
		double pdf = glm::distance2(p, s.value.p) * s.pdf / glm::abs(glm::dot(s.value.n, dir));

		Sample<Ray> sr;
		sr.pdf = pdf * selection_pdf;
		sr.value = Ray(glm::fma(dir, kReflectionBias, p), dir);
		return sr;
	}

	//struct MicroSurfaceIntersection {
	//	MicroSurface surface;
	//};
	inline boost::optional<MicroSurface> intersect(const Ray &ray, const Scene &scene) {
		double tmin = std::numeric_limits<double>::max();
		LazyValue<MicroSurface, 256> min_intersection;

		for (int i = 0; i < scene.objects.size(); ++i) {
			if (auto *s = boost::get<SphereObject>(&scene.objects[i])) {
				if (auto intersection = intersect(ray, s->sphere)) {
					if (intersection->tmin < tmin) {
						min_intersection = [intersection, ray, s]() {
							MicroSurface m;
							m.p = intersection->intersect_position(ray);
							m.n = intersection->intersect_normal(s->sphere.center, m.p);
							m.vn = m.n;
							m.m = s->material;
							m.isback = intersection->isback;

							return m;
						};
						tmin = intersection->tmin;
					}
				}
			} else if (auto *c = boost::get<ConelBoxObject>(&scene.objects[i])) {
				for (int i = 0; i < c->triangles.size(); ++i) {
					if (auto intersection = intersect(ray, c->triangles[i].triangle)) {
						if (intersection->tmin < tmin) {
							min_intersection = [ray, intersection, i, c]() {
								MicroSurface m;
								m.p = intersection->intersect_position(ray);
								m.n = intersection->intersect_normal(c->triangles[i].triangle);
								m.vn = m.n;
								m.m = LambertMaterial(c->triangles[i].color);
								m.isback = intersection->isback;

								return m;
							};
							tmin = intersection->tmin;
						}
					}
				}
			} else if (auto *d = boost::get<DiscLight>(&scene.objects[i])) {
				//if (auto intersection = light->intersect(ray, tmin)) {
				//	min_intersection = [intersection]() {
				//		MicroSurfaceIntersection msi;
				//		msi.surface = *intersection;
				//		return msi;
				//	};
				//}
				if (auto intersection = intersect(ray, d->disc)) {
					if (intersection->tmin < tmin) {
						min_intersection = [ray, intersection, d]() {
							MicroSurface m;
							m.p = intersection->intersect_position(ray);
							m.n = intersection->intersect_normal;
							m.vn = m.n;
							m.m = intersection->isback ? EmissiveMaterial(Vec3(0.0)) : d->emissive;
							m.isback = intersection->isback;

							return m;
						};
						tmin = intersection->tmin;
					}
				}
				/*Plane plane = make_plane_pn(Vec3(0.0, r->y, 0.0), Vec3(0.0, -1.0, 0.0));
				if (auto intersection = intersect(ray, plane)) {
					auto p = intersection->intersect_position(ray);
					if (-r->size * 0.5 < p.x && p.x < r->size * 0.5) {
						if (-r->size * 0.5 < p.z && p.z < r->size * 0.5) {
							if (intersection->tmin < tmin) {
								min_intersection = [p, ray, intersection, i, r]() {
									MicroSurface m;
									m.p = p;
									m.n = intersection->intersect_normal;
									m.vn = m.n;
									m.m = intersection->isback ? EmissiveMaterial(Vec3(0.0)) : r->emissive_material();
									m.isback = intersection->isback;

									MicroSurfaceIntersection msi;
									msi.surface = m;
									return msi;
								};
								tmin = intersection->tmin;
							}
						}
					}
				}*/
			}
		}
		if (tmin != std::numeric_limits<double>::max()) {
			return min_intersection.evaluate();
		}
		return boost::none;
	}
}