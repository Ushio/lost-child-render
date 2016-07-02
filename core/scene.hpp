﻿#pragma once

#include <boost/variant.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/range.hpp>
#include <boost/range/join.hpp>

#include "constants.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "collision_plane.hpp"
#include "material.hpp"

namespace lc {
	struct LightSurface {
		Vec3 n;
		Vec3 p;
	};
	class ILight {
	public:
		virtual ~ILight() {}
		virtual EmissiveMaterial emissive_material() const = 0;
		virtual Sample<LightSurface> on_light(EngineType &e) const = 0;
	};

	struct RectLight : public ILight {
		RectLight() {

		}
		virtual EmissiveMaterial emissive_material() const {
			return EmissiveMaterial(color);
		}
		virtual Sample<LightSurface> on_light(EngineType &e) const {
			Sample<LightSurface> ls;
			ls.pdf = 1.0 / (size * size);
			ls.value.p = Vec3(
				generate_continuous(e, -size * 0.5, size * 0.5),
				y,
				generate_continuous(e, -size * 0.5, size * 0.5));
			ls.value.n = Vec3(0.0, -1.0, 0.0);
			return ls;
		}
		double size = 0.0;
		double y = 0.0;
		Vec3 color;
	};

	struct SphereObject : public ILight {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}

		virtual EmissiveMaterial emissive_material() const {
			return boost::get<EmissiveMaterial>(material);
		}
		virtual Sample<LightSurface> on_light(EngineType &e) const {
			Sample<LightSurface> s;
			s.value.p = sphere.center + generate_on_sphere(e) * sphere.radius;
			s.value.n = glm::normalize(s.value.p - sphere.center);
			s.pdf = 1.0 / (4.0 * glm::pi<double>() * sphere.radius * sphere.radius);
			return s;
		}

		Sphere sphere;
		Material material;
		boost::uuids::uuid object_id = boost::uuids::random_generator()();
	};

	struct TriangleMeshObject {
		BVH bvh;
		Material material;
		Transform transform;
		boost::uuids::uuid object_id = boost::uuids::random_generator()();
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
		boost::uuids::uuid object_id = boost::uuids::random_generator()();
	};

	typedef boost::variant<SphereObject, ConelBoxObject, TriangleMeshObject, RectLight> SceneObject;

	//struct ImportantArea {
	//	ImportantArea() {}
	//	ImportantArea(const Sphere &sphere, const boost::uuids::uuid &uuid) :shape(sphere), object_id(uuid) {}

	//	Sphere shape;
	//	boost::uuids::uuid object_id;
	//	// double importance = 1.0;

	//	// boost::variant<Sphere, AABB> shape;

	//	/* p が重点サンプルの基準点 */
	//	template <class E>
	//	Sample<Vec3> sample(const Vec3 &p, RandomEngine<E> &engine) const {
	//		const double eps = 0.01;
	//		const double pdf_min = 0.01;

	//		double distance_pq;
	//		Vec3 q;
	//		Vec3 qn;
	//		do {
	//			q = shape.center + generate_on_sphere(engine) * shape.radius;
	//			qn = glm::normalize(q - shape.center);
	//			distance_pq = glm::distance(p, q);
	//		} while (distance_pq < eps);

	//		// PDFが小さい => 面積が大きい, 角度がついていない, 距離が短い
	//		Sample<Vec3> s;
	//		s.value = q;
	//		double A = 4.0 * glm::pi<double>() * shape.radius * shape.radius;
	//		double cos_alpha = glm::max(glm::abs(glm::dot(qn, (p - q) / distance_pq)), eps); // あまり小さくなりすぎないように
	//		s.pdf = distance_pq * distance_pq / (A * cos_alpha);
	//		s.pdf = glm::max(s.pdf, pdf_min);
	//		return s;
	//	}
	//};

	

	struct Scene {
		Transform viewTransform;
		Camera camera;
		std::vector<SceneObject> objects;

		std::vector<ILight *> lights;

		//// 直接サンプリング
		//std::vector<ImportantArea> lights;
		//std::vector<ImportantArea> importances;
	};

	struct OnLight {
		Vec3 p;
		Vec3 n;
		double pdf = 0.0;
		EmissiveMaterial emissive;
	};
	OnLight on_light(const Scene &scene, EngineType &engine) {
		const ILight *light = scene.lights.size() == 1 ?
			scene.lights[0]
			:
			scene.lights[engine() % scene.lights.size()];

		Sample<LightSurface> s = light->on_light(engine);

		OnLight onLight;
		onLight.emissive = light->emissive_material();
		onLight.p = s.value.p;
		onLight.n = s.value.n;
		onLight.pdf = s.pdf;
		return onLight;
	}

	struct DirectSample {
		Ray ray;
		double pdf = 0.0;
	};

	inline DirectSample direct_sample(const Scene &scene, const Vec3 &p, EngineType &engine) {
		const ILight *light = scene.lights.size() == 1 ?
			scene.lights[0]
			:
			scene.lights[engine() % scene.lights.size()];


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
		Sample<LightSurface> s = light->on_light(engine);
		Vec3 dir = glm::normalize(s.value.p - p);
		double pdf = glm::distance2(p, s.value.p) * s.pdf / glm::abs(glm::dot(s.value.n, dir));
		// double pdf = glm::distance2(p, s.value.p) * s.pdf / glm::max(glm::dot(s.value.n, -dir), 0.0001);


		DirectSample ds;
		ds.pdf = pdf;
		ds.ray = Ray(glm::fma(dir, kReflectionBias, p), dir);
		return ds;
	}

	//template <class E>
	//DirectSample important_object_sample(const Scene &scene, const Vec3 &p, RandomEngine<E> &engine) {
	//	Sample<Vec3> on_importance = scene.importances.size() == 1 ?
	//		scene.importances[0].sample(p, engine)
	//		:
	//		scene.importances[engine() % scene.importances.size()].sample(p, engine);
	//	Vec3 direction = glm::normalize(on_importance.value - p);

	//	DirectSample ds;
	//	ds.pdf = on_importance.pdf;
	//	ds.ray = Ray(glm::fma(direction, kReflectionBias, p), direction);
	//	return ds;
	//}

	//template <class E>
	//boost::optional<Sample<Vec3>> sample_important_position(const Scene &scene, const Vec3 &p, RandomEngine<E> &engine, double diffusion) {
	//	if (scene.lights.empty() && scene.importances.empty()) {
	//		return boost::none;
	//	}

	//	// モジュロバイアスはNが十分に小さいので無視する
	//	if (scene.importances.empty() && scene.lights.empty() == false) {
	//		return scene.lights.size() == 1 ?
	//			scene.lights[0].sample(p, engine)
	//			:
	//			scene.lights[engine() % scene.lights.size()].sample(p, engine);
	//	}
	//	if (scene.importances.empty() == false && scene.lights.empty()) {
	//		return scene.importances.size() == 1 ?
	//			scene.importances[0].sample(p, engine)
	//			:
	//			scene.importances[engine() % scene.importances.size()].sample(p, engine);
	//	}

	//	// 探索が深ければ、ライトサンプリングの確率は上げたほうがいい
	//	bool is_light_sampleing = 0.25 * glm::pow(0.25, diffusion) < generate_continuous(engine);
	//	if (is_light_sampleing) {
	//		return scene.lights.size() == 1 ?
	//			scene.lights[0].sample(p, engine)
	//			:
	//			scene.lights[engine() % scene.lights.size()].sample(p, engine);
	//	}
	//	return scene.importances.size() == 1 ?
	//		scene.importances[0].sample(p, engine)
	//		:
	//		scene.importances[engine() % scene.importances.size()].sample(p, engine);
	//}
/*
	inline bool is_important(const Scene &scene, const boost::uuids::uuid &object_id) {
		bool important = false;
		for (const ImportantArea &area : boost::join(scene.lights, scene.importances)) {
			if (area.object_id == object_id) {
				important = true;
				break;
			}
		}
		return important;
	}
*/
	template <class T, int MAX_SIZE>
	struct LazyValue {
		LazyValue() {}
		LazyValue(const LazyValue &) = delete;
		void operator=(const LazyValue &) = delete;

		~LazyValue() {
			if (_hasValue) {
				static_cast<HolderErase *>(_storage.address())->~HolderErase();
			}
		}

		struct HolderErase {
			virtual ~HolderErase() {}
			virtual T evaluate() const = 0;
		};

		template <class F>
		struct Holder : public HolderErase {
			Holder(const F &f) :_f(f) {}
			T evaluate() const {
				return _f();
			}
			F _f;
		};

		template <class F>
		void operator=(const F &f) {
			static_assert(sizeof(F) <= StorageType::size, "low memory");
			if (_hasValue) {
				static_cast<HolderErase *>(_storage.address())->~HolderErase();
			}
			new (_storage.address()) Holder<F>(f);
			_hasValue = true;
		}
		bool hasValue() const {
			return _hasValue;
		}
		T evaluate() const {
			return static_cast<const HolderErase *>(_storage.address())->evaluate();
		}

		typedef boost::aligned_storage<MAX_SIZE, 8> StorageType;

		bool _hasValue = false;
		StorageType _storage;
	};

	struct MicroSurfaceIntersection {
		MicroSurface surface;
		boost::uuids::uuid object_id;
	};
	inline boost::optional<MicroSurfaceIntersection> intersect(const Ray &ray, const Scene &scene) {
		double tmin = std::numeric_limits<double>::max();
		LazyValue<MicroSurfaceIntersection, 256> min_intersection;

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

							MicroSurfaceIntersection msi;
							msi.surface = m;
							msi.object_id = s->object_id;
							return msi;
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

								MicroSurfaceIntersection msi;
								msi.surface = m;
								msi.object_id = c->object_id;
								return msi;
							};
							tmin = intersection->tmin;
						}
					}
				}
			} else if (auto *r = boost::get<RectLight>(&scene.objects[i])) {
				Plane plane = make_plane_pn(Vec3(0.0, r->y, 0.0), Vec3(0.0, -1.0, 0.0));
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
									// msi.object_id = c->object_id;
									return msi;
								};
								tmin = intersection->tmin;
							}
						}
					}
				}
			}
		}
		if (tmin != std::numeric_limits<double>::max()) {
			return min_intersection.evaluate();
		}
		return boost::none;
	}
	/*inline boost::optional<MicroSurface> intersect(const Ray &ray, const Scene &scene) {
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
			else if (auto *c = boost::get<ConelBoxObject>(&scene.objects[i])) {
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
			else if (auto *c = boost::get<TriangleMeshObject>(&scene.objects[i])) {
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
	}*/
}