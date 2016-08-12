#pragma once

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
#include "uniform_on_triangle.hpp"

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
		virtual bool is_visible(const Ray &ray, double tmin_target) const = 0;
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
				}
				s.value.n = -s.value.n;
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
		bool is_visible(const Ray &ray, double tmin_target) const override {
			if (auto intersection = lc::intersect(ray, disc)) {
				if (intersection->tmin < tmin_target) {
					return false;
				}
			}
			return true;
		}

		Disc disc;
		EmissiveMaterial emissive;
		bool doubleSided = false;
	};

	struct PolygonLight : public ILight {
		Sample<OnLight> sample(DefaultEngine &e, const Vec3 &srcP) const override {
			auto u = uniform_triangle.uniform(e);
			Sample<OnLight> s;
			s.pdf = 1.0 / uniform_triangle.get_area();
			s.value.p = u.p;
			s.value.n = triangle_normal(uniform_triangle._triangles[u.index], false);

			bool isBack = 0.0 < glm::dot(s.value.n, u.p - srcP);
			s.value.emissive = isBack ? emissive_back : emissive_front;

			// サンプルソースの方向と法線が反対を向いてしまっている
			if (isBack) {
				s.value.n = -s.value.n;
			}

			return s;
		}
		double getArea() const override {
			return uniform_triangle.get_area();
		}

		void intersect(const Ray &ray, LazyMicroSurface &surface, double &tmin) const override {
			if (auto intersection = bvh.intersect(ray, tmin)) {
				if (intersection->tmin < tmin) {
					auto triangle = uniform_triangle._triangles[intersection->triangle_index];
					EmissiveMaterial emissive_front_value = emissive_front;
					EmissiveMaterial emissive_back_value = emissive_back;

					surface = [ray, intersection, triangle, emissive_front_value, emissive_back_value]() {
						MicroSurface m;
						m.p = intersection->intersect_position(ray);
						m.n = intersection->intersect_normal(triangle);
						m.vn = m.n;
						m.m = intersection->isback ? emissive_back_value : emissive_front_value;
						m.isback = intersection->isback;
						return m;
					};

					tmin = intersection->tmin;
				}
			}
		}
		bool is_visible(const Ray &ray, double tmin_target) const override {
			return bvh.is_visible(ray, tmin_target);
		}

		EmissiveMaterial emissive_front;
		EmissiveMaterial emissive_back;

		UniformOnTriangle uniform_triangle;
		BVH bvh;
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
		bool is_visible(const Ray &ray, double tmin_target) const override {
			if (auto intersection = lc::intersect(ray, sphere)) {
				if (intersection->tmin < tmin_target) {
					return false;
				}
			}
			return true;
		}
	};

	struct MeshObject : public ISceneIntersectable {
		BVH bvh;
		Material material;

		void intersect(const Ray &ray, LazyMicroSurface &surface, double &tmin) const override {
			if (auto intersection = bvh.intersect(ray, tmin)) {
				if (intersection->tmin < tmin) {
					auto triangle = bvh._triangles[intersection->triangle_index];
					Material material_value = material;

					surface = [ray, intersection, triangle, material_value]() {
						MicroSurface m;
						m.p = intersection->intersect_position(ray);
						m.n = intersection->intersect_normal(triangle);
						m.vn = m.n;
						m.m = material_value;
						m.isback = intersection->isback;
						return m;
					};
					tmin = intersection->tmin;
				}
			}
		}
		bool is_visible(const Ray &ray, double tmin_target) const override {
			return bvh.is_visible(ray, tmin_target);
		}
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
		bool is_visible(const Ray &ray, double tmin_target) const override {
			for (int i = 0; i < triangles.size(); ++i) {
				if (lc::is_visible(ray, triangles[i].triangle, tmin_target) == false) {
					return false;
				}
			}
			return true;
		}
	};

	typedef boost::variant<SphereObject, ConelBoxObject, MeshObject, DiscLight, PolygonLight> SceneObject;

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
	struct DirectSampling {
		double tmin = 0.0;
		Ray ray;
		OnLight onLight;
	};
	inline Sample<DirectSampling> direct_light_sample(const Scene &scene, const Vec3 &p, DefaultEngine &engine) {
		const ILight *light = scene.lights.size() == 1 ?
			scene.lights[0]
			:
			scene.lights[engine.generate() % scene.lights.size()];

		double selection_pdf = 1.0 / scene.lights.size();

		// 表面積の確率密度を立体角の確率密度に変換する
		Sample<OnLight> s = light->sample(engine, p);
		double distance_squared = glm::distance2(p, s.value.p);
		double dist = glm::sqrt(distance_squared);
		Vec3 dir = (s.value.p - p) / dist;
		double pdf = distance_squared * s.pdf / glm::abs(glm::dot(s.value.n, dir));

		Sample<DirectSampling> ds;
		ds.pdf = pdf * selection_pdf;
		ds->ray = Ray(glm::fma(dir, kReflectionBias, p), dir);
		ds->onLight = s.value;
		ds->tmin = dist;
		
		// 法線を調整
		//if (0.0 < glm::dot(ds->onLight.n, dir)) {
		//	ds->onLight.n = -ds->onLight.n;
		//}

		return ds;
	}

	// シーンとレイの衝突判定
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

	// rayからtmin_targetまでの遮蔽をしらべる
	// いくらかこちらのほうが計算を省略できる
	inline bool is_visible(const Ray &ray, const Scene &scene, double tmin_target) {
		double tmin = tmin_target;
		LazyValue<MicroSurface, 256> min_intersection;

		for (int i = 0; i < scene.objects.size(); ++i) {
			if (auto *intersectable = boost::polymorphic_strict_get<ISceneIntersectable>(&scene.objects[i])) {
				if (intersectable->is_visible(ray, tmin_target) == false) {
					return false;
				}
				//intersectable->intersect(ray, min_intersection, tmin);
				//// tmin_targetよりも手前に何か存在していた
				//if (tmin + 0.00001 < tmin_target) {
				//	return false;
				//}
			}
		}
		return true;
	}
}