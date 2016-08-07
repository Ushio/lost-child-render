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
	typedef LazyValue<MicroSurface, 256> LazyMicroSurface;

	struct OnLight {
		Vec3 p;
		Vec3 n;
		EmissiveMaterial emissive;
	};
	class ISceneIntersectable {
	public:
		virtual ~ISceneIntersectable() {}
		virtual void intersect(const Ray &ray, LazyMicroSurface &surface, double &tmin) const = 0;
	};
	class ILight : public ISceneIntersectable {
	public:
		virtual ~ILight() {}

		// これは
		virtual Sample<OnLight> sample(DefaultEngine &e, const Vec3 &srcP) const = 0;
		virtual double getArea() const = 0;
	};

	struct DiscLight : public ILight {
		DiscLight() {

		}

		Sample<OnLight> sample(DefaultEngine &e, const Vec3 &srcP) const override {
			HemisphereTransform hemisphereTransform(disc.plane.n);
			auto circle = e.on_circle() * disc.radius;
			auto p = disc.origin + hemisphereTransform.transform(Vec3(circle.x, 0.0, circle.y));

			Sample<OnLight> s;
			s.pdf = 1.0 / this->getArea();
			s.value.p = p;
			s.value.n = disc.plane.n;
			s.value.emissive = emissive;

			// サンプルソースの方向と法線が反対を向いてしまっている
			if (0.0 < glm::dot(disc.plane.n, p - srcP)) {
				if (doubleSided == false) {
					s.value.emissive = EmissiveMaterial(Vec3(0.0));
				} else {
					s.value.n = -s.value.n;
				}
			}

			return s;
		}
		double getArea() const override {
			return glm::pi<double>() * disc.radius * disc.radius;
		}

		void intersect(const Ray &ray, LazyMicroSurface &surface, double &tmin) const override {
			if (auto intersection = lc::intersect(ray, disc)) {
				if (intersection->tmin < tmin) {
					auto emissive_value = this->emissive;
					auto doubleSided_value = doubleSided;
					surface = [ray, intersection, emissive_value, doubleSided_value]() {
						MicroSurface m;
						m.p = intersection->intersect_position(ray);
						m.n = intersection->intersect_normal;

						// 反対を向いたときの処理
						if (doubleSided_value) {
							if (intersection->isback) {
								m.n = -m.n;
							}
							m.m = emissive_value;
						} else {
							m.m = intersection->isback ? EmissiveMaterial(Vec3(0.0)) : emissive_value;
						}

						m.vn = m.n;
						m.isback = intersection->isback;
						return m;
					};

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

	struct SphereObject : public ISceneIntersectable {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}
		Sphere sphere;
		Material material;

		void intersect(const Ray &ray, LazyMicroSurface &surface, double &tmin) const override {
			if (auto intersection = lc::intersect(ray, sphere)) {
				if (intersection->tmin < tmin) {
					auto s = sphere;
					auto mat = material;
					surface = [intersection, ray, s, mat]() {
						MicroSurface m;
						m.p = intersection->intersect_position(ray);
						m.n = intersection->intersect_normal(s.center, m.p);
						m.vn = m.n;
						m.m = mat;
						m.isback = intersection->isback;
						return m;
					};
					tmin = intersection->tmin;
				}
			}
		}
	};

	struct TriangleMeshObject {
		BVH bvh;
		Material material;
		Transform transform;
	};

	struct ConelBoxObject : public ISceneIntersectable {
		ConelBoxObject() :ConelBoxObject(5.0) {}
		~ConelBoxObject() {}
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

		void intersect(const Ray &ray, LazyMicroSurface &surface, double &tmin) const override {
			for (int i = 0; i < triangles.size(); ++i) {
				if (auto intersection = lc::intersect(ray, triangles[i].triangle)) {
					if (intersection->tmin < tmin) {
						ColorTriangle triangle = triangles[i];
						surface = [ray, intersection, triangle]() {
							MicroSurface m;
							m.p = intersection->intersect_position(ray);
							m.n = intersection->intersect_normal(triangle.triangle);
							m.vn = m.n;
							m.m = LambertMaterial(triangle.color);
							m.isback = intersection->isback;

							return m;
						};
						tmin = intersection->tmin;
					}
				}
			}
		}
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
				if (auto *light = boost::polymorphic_strict_get<ILight>(&objects[i])) {
					lights.push_back(light);
				}
			}
		}

		std::vector<SceneObject> objects;
		std::vector<ILight *> lights;
	};

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

		// 表面積の確率密度を立体角の確率密度に変換する
		Sample<OnLight> s = light->sample(engine, p);
		Vec3 dir = glm::normalize(s.value.p - p);
		double pdf = glm::distance2(p, s.value.p) * s.pdf / glm::abs(glm::dot(s.value.n, dir));

		Sample<Ray> sr;
		sr.pdf = pdf * selection_pdf;
		sr.value = Ray(glm::fma(dir, kReflectionBias, p), dir);
		return sr;
	}

	inline boost::optional<MicroSurface> intersect(const Ray &ray, const Scene &scene) {
		double tmin = std::numeric_limits<double>::max();
		LazyValue<MicroSurface, 256> min_intersection;

		for (int i = 0; i < scene.objects.size(); ++i) {
			if (auto *intersectable = boost::polymorphic_strict_get<ISceneIntersectable>(&scene.objects[i])) {
				intersectable->intersect(ray, min_intersection, tmin);
			}
		}

		if (tmin != std::numeric_limits<double>::max()) {
			return min_intersection.evaluate();
		}
		return boost::none;
	}
}