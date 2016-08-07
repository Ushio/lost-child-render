#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include <ppl.h>

#include "constants.hpp"
#include "collision_triangle.hpp"
#include "bvh.hpp"
#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "helper_cinder/draw_camera.hpp"
#include "helper_cinder/draw_scene.hpp"
#include "importance.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "refraction.hpp"
#include "material.hpp"
#include "scene.hpp"
#include "brdf.hpp"
#include "constants.hpp"

#include <stack>
#include <chrono>

#include <boost/format.hpp>
#include <boost/variant.hpp>

static const int wide = 256;

namespace lc {
	struct AccumlationBuffer {
		AccumlationBuffer(int width, int height, int random_skip = 50):_width(width), _height(height), _data(width * height){
			for (int y = 0; y < _height; ++y) {
				for (int x = 0; x < _width; ++x) {
					int index = y * _width + x;
					Pixel &pixel = _data[index];
					pixel.engine = DefaultEngine(index + 1);
					pixel.engine.discard(random_skip);
				}
			}
		}
		struct Pixel {
			Vec3 color;
			DefaultEngine engine;
		};

		int _width = 0;
		int _height = 0;
		std::vector<Pixel> _data;
		int _iteration = 0;
		int _ray_count = 0;
	};


	struct Context {
		int depth = 0;
		bool is_direct_sample = false;
		double diffusion = 0.0;
		Vec3 color_weight = Vec3(1.0);

		Context &step() {
			depth += 1;
			return *this;
		}
		Context &direct() {
			is_direct_sample = true;
			return *this;
		}
		Context &diffuse(double d) {
			diffusion += d;
			return *this;
		}
		Context &albedo(const Vec3 &albedo) {
			color_weight *= albedo;
			return *this;
		}
	};

	inline bool visible(const Scene &scene, const Vec3 &p0, const Vec3 &p1) {
		Vec3 d = p1 - p0;
		double lengthSquared = glm::length2(d);
		if (lengthSquared < 0.001) {
			// 遮蔽していることにしたほうが都合がよいかも？
			return false;
		}
		if (auto intersection = intersect(Ray(p0, d / glm::sqrt(lengthSquared)), scene)) {
			return glm::distance2(intersection->p, p1) < glm::epsilon<double>();
		}
		return false;
	}

	struct Path {
		struct Node {
			Vec3 omega_i;
			Vec3 omega_o;

			// ここまでのパスを活用する場合の係数
			// 注意: ここ自身の係数は入っていない
			// なぜなら、結合やNEEで係数が変化するため、パスを使う側でそこは計算する
			Vec3 coef = Vec3(1.0);

			// ここまでのパスを活用する場合の確率密度
			// 注意: ここ自身の確率密度は入っていない
			// なぜなら、結合やNEEで係数が変化するため、パスを使う側でそこは計算する
			double pdf = 1.0;

			MicroSurface surface;
		};
		std::vector<Node> nodes;
	};
	
	inline Path path_trace(const Ray &ray, const Scene &scene, DefaultEngine &engine) {
		Ray curr_ray = ray;
		Path path;

		Vec3 coef(1.0);
		double pdf = 1.0;
		int diffusion_count = 0;
		Vec3 diffusion(1.0);

		int max_trace = 5;
		int max_diffusion_count = 3;
		path.nodes.reserve(max_trace);

		for (int i = 0; i < max_trace && diffusion_count < max_diffusion_count; ++i) {
			auto intersection = intersect(curr_ray, scene);
			if (!intersection) {
				break;
			}
			auto surface = *intersection;

			Vec3 omega_o = -curr_ray.d;
			if (auto lambert = boost::get<LambertMaterial>(&surface.m)) {
				diffusion_count++;

				auto eps = std::make_tuple(engine.continuous(), engine.continuous());
				Sample<Vec3> lambert_sample = importance_lambert(eps, surface.n);
				Vec3 omega_i = lambert_sample.value;
				Vec3 omega_o = -curr_ray.d;

				double this_pdf = lambert_sample.pdf;

				Path::Node node;
				node.coef = coef;
				node.pdf = pdf;
				node.surface = surface;
				node.omega_i = omega_i;
				node.omega_o = omega_o;
				path.nodes.push_back(node);

				// BRDF 
				double brdf = glm::one_over_pi<double>();
				double cos_term = glm::max(glm::dot(surface.n, omega_i), 0.0);
				Vec3 this_coef = lambert->albedo * brdf * cos_term;

				coef *= this_coef;
				pdf *= this_pdf;

				curr_ray = Ray(glm::fma(omega_o, kReflectionBias, surface.p), omega_i);
				continue;
			} else if(auto cook = boost::get<CookTorranceMaterial>(&surface.m)) {
				diffusion_count += cook->roughness;

				auto eps = std::make_tuple(engine.continuous(), engine.continuous());
				Sample<GGXValue> ggx_sample = importance_ggx(eps, surface.n, omega_o, cook->roughness);

				Vec3 n = surface.n;
				Vec3 omega_i = ggx_sample.value.omega_i;
				Vec3 omega_o = -curr_ray.d;

				// これでいいのか？
				// 遮蔽という意味ではいい気もする
				if (glm::dot(omega_i, n) < 0.0) {
					return Path();
				}
				double this_pdf = ggx_sample.pdf;

				double cos_term = glm::max(glm::dot(n, omega_i), 0.0);
				double f = lc::fresnel(cos_term, cook->fesnel_coef);

				double brdf = 0.0;

				//double g = G(omega_i, omega_o, ggx_sample.value.h, surface.n, cook->roughness);
				//double d = ggx_d(glm::dot(ggx_sample.value.h, surface.n), cook->roughness);
				//brdf = d * f * g / glm::max(4.0 * glm::dot(omega_o, surface.n) * cos_term, 0.0001);
				 
				// フレネルによるBRDFブレンディング
				if (engine.continuous() < f) {
					double g = G(omega_i, omega_o, ggx_sample.value.h, n, cook->roughness);
					double d = ggx_d(glm::dot(ggx_sample.value.h, n), cook->roughness);
					brdf = d * g / glm::max(4.0 * glm::dot(omega_o, n) * cos_term, kEPS);
				}
				else {
					// brdf = 0;
					auto eps = std::make_tuple(engine.continuous(), engine.continuous());
					Sample<Vec3> lambert_sample = importance_lambert(eps, surface.n);
					omega_i = lambert_sample.value;
					omega_o = -curr_ray.d;
					this_pdf = lambert_sample.pdf;

					brdf = glm::one_over_pi<double>();

					cos_term = glm::max(glm::dot(n, omega_i), 0.0);
				}

				Path::Node node;
				node.coef = coef;
				node.pdf = pdf;
				node.surface = surface;
				node.omega_i = omega_i;
				node.omega_o = omega_o;
				path.nodes.push_back(node);

				Vec3 this_coef = cook->albedo * brdf * cos_term;

				coef *= this_coef;
				pdf *= this_pdf;

				curr_ray = Ray(glm::fma(omega_o, kReflectionBias, surface.p), omega_i);
				continue;
			} else if(auto refrac = boost::get<RefractionMaterial>(&surface.m)) {
				double eta = surface.isback ? refrac->ior / 1.0 : 1.0 / refrac->ior;
				double fresnel_value = fresnel(dot(omega_o, surface.n), 0.02);
				if (fresnel_value < engine.continuous()) {
					auto omega_i_refract = refraction(-omega_o, surface.n, eta);
					curr_ray = Ray(glm::fma(omega_i_refract, kReflectionBias, surface.p), omega_i_refract);
					continue;
				}
				else {
					auto omega_i_reflect = glm::reflect(-omega_o, surface.n);
					curr_ray = Ray(glm::fma(omega_i_reflect, kReflectionBias, surface.p), omega_i_reflect);
					continue;
				}
			}
			else if (auto specular = boost::get<PerfectSpecularMaterial>(&surface.m)) {
				auto omega_i_reflect = glm::reflect(-omega_o, surface.n);
				curr_ray = Ray(glm::fma(omega_i_reflect, kReflectionBias, surface.p), omega_i_reflect);
				continue;
			} else if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
				if (glm::dot(surface.n, curr_ray.d) < 0.0) {
					Path::Node node;
					node.coef = coef;
					node.pdf = pdf;
					// node.omega_i = 存在しない
					node.omega_o = -curr_ray.d;
					node.surface = surface;
					path.nodes.push_back(node);
				}
				else {
					// 裏面はいったん気にしないことにする
				}
				
				break;
			}


		}
		return path;
	}

	//inline Vec3 evaluate_bi_directional(const Path &camera_path, const Path &light_path, int camera_i, int light_i, double on_light_pdf) {
	//	// カメラパスの最後（接続点）
	//	Path::Node camera_path_tail = camera_path.nodes[camera_i];

	//	// ライトパスの頭（接続点）
	//	Path::Node light_path_head = light_path.nodes[light_i];

	//	Vec3 coef(1.0);

	//	coef *= camera_path_tail.coef;
	//	coef *= light_path_head.coef;

	//	Vec3 connection_direction = light_path_head.surface.p - camera_path_tail.surface.p;
	//	double lengthSquared = glm::length2(connection_direction);
	//	connection_direction /= glm::sqrt(lengthSquared);

	//	double geometry =
	//		glm::max(glm::dot(connection_direction, camera_path_tail.surface.n), 0.0001)
	//		*
	//		glm::max(glm::dot(-connection_direction, light_path_head.surface.n), 0.0001)
	//		/
	//		lengthSquared;

	//	if (lengthSquared < 0.00001) {
	//		return Vec3();
	//	}

	//	coef *= geometry;

	//	// 二つのBRDF関係を再計算
	//	{
	//		if (auto lambert = boost::get<LambertMaterial>(&camera_path_tail.surface.m)) {
	//			auto surface = camera_path_tail.surface;
	//			Vec3 omega_i = connection_direction;
	//			Vec3 omega_o = camera_path_tail.omega_o;

	//			double brdf = glm::one_over_pi<double>();
	//			double cos_term = glm::max(glm::dot(surface.n, omega_i), 0.0); // マイナスがいるようだが、原因は不明
	//			Vec3 this_coef = lambert->albedo * brdf * cos_term;
	//			coef *= this_coef;
	//		}
	//	}

	//	{
	//		if (auto lambert = boost::get<LambertMaterial>(&light_path_head.surface.m)) {
	//			auto surface = light_path_head.surface;
	//			Vec3 omega_i = camera_path_tail.omega_i;
	//			Vec3 omega_o = -connection_direction;

	//			double brdf = glm::one_over_pi<double>();
	//			double cos_term = glm::max(glm::dot(surface.n, omega_i), 0.0); // マイナスがいるようだが、原因は不明
	//			Vec3 this_coef = lambert->albedo * brdf * cos_term;
	//			coef *= this_coef;
	//		}
	//	}

	//	// light_path.nodes[0].surface.
	//	//Vec3 light_p = light_path.nodes[0].surface.p;
	//	//Vec3 light_n = light_path.nodes[0].surface.n;
	//	//Vec3 direct_sample_p;
	//	//if (light_i == 0) {
	//	//	direct_sample_p = camera_path.nodes[camera_i].surface.p;
	//	//}
	//	//else {
	//	//	direct_sample_p = light_path.nodes[light_i].surface.p;
	//	//}
	//	//Vec3 light_dir = glm::normalize(light_p - direct_sample_p);
	//	//double light_pdf = glm::distance2(light_p, direct_sample_p) * on_light_pdf / glm::max(glm::dot(light_n, light_dir), 0.0001);
	//	//coef /= light_pdf;

	//	return coef;
	//}

	inline Vec3 radiance(const Ray &camera_ray, const Scene &scene, DefaultEngine &engine) {
		// 通常のパストレーシング
		Path camera_path = path_trace(camera_ray, scene, engine);

		// ライトからのレイを生成する
		// OnLight light = on_light(scene, engine);
		
		// Ray light_ray = Ray(light.p, generate_on_sphere(engine));

		// TODO だいぶアドホック
		//if (0.0 < light_ray.d.y) {
		//	light_ray.d.y = -light_ray.d.y;
		//}

		//light_ray.o = glm::fma(light_ray.d, kReflectionBias, light_ray.o);


		// ライトトレーシング
		/*MicroSurface lightSurface;
		lightSurface.p = light.p;
		lightSurface.m = light.emissive;
		lightSurface.n = light.n;
		Path light_path = trace(light_ray, scene, engine, lightSurface);*/

		// そもそもカメラレイが衝突していない
		if (camera_path.nodes.empty()) {
			return Vec3();
		}

		// そもそも最初がライトだった特殊ケース
		if (auto emissive = boost::get<EmissiveMaterial>(&camera_path.nodes[0].surface.m)) {
			return emissive->color;
		}

		// なんか単位に対するものが変な気がする
		//double light_pdf = 1.0;
		//light_pdf *= light.pdf;


		// 向きを決めるPDF
		// light_pdf *= (1.0 / (4.0 * glm::pi<double>()));

		// 暗黙的
		//auto terminal_node = camera_path.nodes[camera_path.nodes.size() - 1];
		//if (auto emissive = boost::get<EmissiveMaterial>(&terminal_node.surface.m)) {
		//	return emissive->color * terminal_node.coef / terminal_node.pdf;
		//}
		//else {
		//	return Vec3();
		//}

		Vec3 color;
		

		for (int ci = 0; ci < camera_path.nodes.size(); ++ci) {
			bool is_term = ci + 1 == camera_path.nodes.size();

			// 暗黙的寄与
			Vec3   implicit_contribution;
			double implicit_pdf = 0.0;

			// 明示的寄与
			Vec3   explicit_contribution;
			double explicit_pdf = 0.0;

			Path::Node camera_node = camera_path.nodes[ci];
			if (auto lambert = boost::get<LambertMaterial>(&camera_node.surface.m)) {
				auto sample_ray = direct_sample_ray(scene, camera_node.surface.p, engine);
				if (auto direct_intersect = intersect(sample_ray.value, scene)) {
					if (auto emissive = boost::get<EmissiveMaterial>(&direct_intersect->m)) {
						double pdf = camera_node.pdf * sample_ray.pdf;
						Vec3 omega_i = sample_ray.value.d;
						double brdf = glm::one_over_pi<double>();
						double cos_term = glm::max(glm::dot(camera_node.surface.n, omega_i), 0.0);
						Vec3 this_coef = lambert->albedo * brdf * cos_term;

						explicit_contribution = this_coef * emissive->color * camera_node.coef / glm::max(pdf, kEPS);
						explicit_pdf = pdf;
					}
				}
			} else if(auto cook = boost::get<CookTorranceMaterial>(&camera_node.surface.m)) {
				auto sample_ray = direct_sample_ray(scene, camera_node.surface.p, engine);
				if (auto direct_intersect = intersect(sample_ray.value, scene)) {
					if (auto emissive = boost::get<EmissiveMaterial>(&direct_intersect->m)) {
						double pdf = camera_node.pdf * sample_ray.pdf;

						Vec3 n = camera_node.surface.n;
						Vec3 omega_o = camera_node.omega_o;
						Vec3 omega_i = sample_ray.value.d;
						double cos_term = glm::max(glm::dot(n, omega_i), 0.0);
						double f = lc::fresnel(cos_term, cook->fesnel_coef);

						double brdf = 0.0;
						if(engine.continuous() < f) {
							Vec3 h = glm::normalize(omega_i + omega_o);
							double g = G(omega_i, omega_o, h, n, cook->roughness);
							double d = ggx_d(glm::dot(h, n), cook->roughness);
							brdf = d * g / glm::max(4.0 * glm::dot(omega_o, n) * cos_term, kEPS);
						}
						else {
							brdf = glm::one_over_pi<double>();
						}
			
						Vec3 this_coef = cook->albedo * brdf * cos_term;

						explicit_contribution = this_coef * emissive->color * camera_node.coef / glm::max(pdf, kEPS);
						explicit_pdf = pdf;
					}
				}
			}
			if (is_term) {
				if (auto emissive = boost::get<EmissiveMaterial>(&camera_node.surface.m)) {
					implicit_contribution = emissive->color * camera_node.coef / glm::max(camera_node.pdf, kEPS);
					implicit_pdf = camera_node.pdf;
				}
			}

			// テスト用 強制暗黙戦略
			//color += implicit_contribution;
			//continue;

			// return implicit_contribution;
			// return explicit_contribution;

#define VISUALIZE_MIS 0

			double weight_all = 0.0;
			Vec3 contribution;

#if VISUALIZE_MIS == 0
			// MIS
			//contribution += implicit_contribution * implicit_pdf;
			//weight_all += implicit_pdf;

			//contribution += explicit_contribution * explicit_pdf;
			//weight_all += explicit_pdf;

			//if (0.0001 < weight_all) {
			//	contribution /= weight_all;
			//	color += contribution;
			//}

			double implicit_weight = glm::pow(implicit_pdf, 2);
			double explicit_weight = glm::pow(explicit_pdf, 2);

			contribution += implicit_contribution * implicit_weight;
			weight_all += implicit_weight;

			contribution += explicit_contribution * explicit_weight;
			weight_all += explicit_weight;

			if (0.0001 < weight_all) {
				contribution /= weight_all;
				color += contribution;
			}

			// MIS ビジュアライズ
			//contribution += Vec3(1.0, 0.0, 0.0) * implicit_pdf;
			//weight_all += implicit_pdf;

			//contribution += Vec3(0.0, 0.0, 1.0) * explicit_pdf;
			//weight_all += explicit_pdf;

			//if (0.0001 < weight_all) {
			//	contribution /= weight_all;
			//	color += contribution;
			//}
#else
			// MIS ビジュアライズ
			contribution += Vec3(1.0, 0.0, 0.0) * implicit_pdf;
			weight_all += implicit_pdf;

			contribution += Vec3(0.0, 0.0, 1.0) * explicit_pdf;
			weight_all += explicit_pdf;

			if (0.0001 < weight_all) {
				contribution /= weight_all;
				color += contribution;
			}
#endif
			
			// BDPT
			//for (int li = 0; li < light_path.nodes.size(); ++li) {
			//	auto a = camera_path.nodes[ci].surface.p;
			//	auto b = light_path.nodes[li].surface.p;

			//	// 高い分散が発生する原因はG項が距離が近すぎると爆発してしまうことにある
			//	if (9.0 < glm::distance2(a, b) && visible(scene, a, b)) {
			//		Vec3 coef = evaluate_bi_directional(camera_path, light_path, ci, li, light.pdf);
			//		double w = weight(ci + 1, li + 1);
			//		color += coef * light.emissive.color * w / light_pdf;
			//		weight_all += w;
			//	}
			//}
		}

		

		return color;
		// return weight_all <= glm::epsilon<double>() ? Vec3() : color / weight_all;
	}

	//inline Vec3 radiance(const Ray &camera_ray, const Scene &scene, DefaultEngine &engine) {
	//	// カメラトレーシング
	//	Path from_camera = trace(camera_ray, scene, engine);
	//	
	//	// ライトからのレイを生成する
	//	OnLight light = on_light(scene, engine);
	//	Ray light_ray = Ray(light.p, generate_on_sphere(engine));

	//	// ライトトレーシング
	//	Path from_light = trace(light_ray, scene, engine);

	//	//// 出発点を決めるPDF
	//	// from_light.pdf *= light.pdf;
	//	// double on_light_pdf = light.pdf;

	//	//// 向きを決めるPDF
	//	// from_light.pdf *= (1.0 / (4.0 * glm::pi<double>()));

	//	// マージする
	//	Path final_path = from_camera;
	//	
	//	if (final_path.nodes.empty()) {
	//		// そもそもカメラレイが衝突していない
	//		return Vec3();
	//	}

	//	bool already_done = boost::get<EmissiveMaterial>(&final_path.nodes[final_path.nodes.size() - 1].ms.m) != nullptr;
	//	if (already_done == false) {
	//		if (from_light.nodes.empty()) {
	//			// そもそもライトレイが衝突していない
	//			return Vec3();
	//		}
	//		// 接続する条件がそろった
	//		MicroSurface camera_end = final_path.nodes[final_path.nodes.size() - 1].ms;
	//		MicroSurface light_begin = from_light.nodes[from_light.nodes.size() - 1].ms;

	//		Ray connect_ray = Ray(camera_end.p, glm::normalize(light_begin.p - camera_end.p));
	//		
	//		auto intersection = intersect(connect_ray, scene);
	//		if (!intersection) {
	//			// 接続に失敗
	//			return Vec3();
	//		}

	//		if (glm::epsilon<double>() < glm::distance2(intersection->surface.p, light_begin.p)) {
	//			// 接続に失敗
	//			return Vec3();
	//		}

	//		while (from_light.nodes.empty() == false) {
	//			final_path.nodes.push_back(from_light.nodes[from_light.nodes.size() - 1]);
	//			from_light.nodes.pop_back();
	//		}

	//		// 最後にライトマテリアルを挿入
	//		// 法線などはライトでは無視するので良い
	//		MicroSurface light_surface;
	//		light_surface.m = light.emissive;
	//		light_surface.p = light.p;

	//		Path::Node node;
	//		node.pdf = 1.0;
	//		node.ms = light_surface;
	//		final_path.nodes.push_back(node);
	//	}
	//	// 片方向ですでにトレースが完了していたのでスキップ
	//	
	//	Vec3 origin = camera_ray.o;
	//	Vec3 omega_o = camera_ray.d;
	//	Vec3 coef(1.0);

	//	double pdf = 1.0;

	//	for (int i = 0; i < final_path.nodes.size(); ++i) {
	//		Path::Node node = final_path.nodes[i];
	//		MicroSurface surface = node.ms;

	//		if (0 <= i && i < final_path.nodes.size() - 1) {
	//			Vec3 p0 = origin;
	//			Vec3 p1 = surface.p;
	//			Vec3 p2 = final_path.nodes[i + 1].ms.p;

	//			Vec3 d0 = glm::normalize(p0 - p1);
	//			Vec3 d1 = glm::normalize(p2 - p1);
	//			Vec3 n = surface.n;

	//			double a = glm::dot(d0, n) * glm::distance2(p2, p1);
	//			double b = glm::dot(d1, n) * glm::distance2(p0, p1);
	//			double px = b / a;
	//			pdf *= px * node.pdf;
	//		}

	//		if (auto lambert = boost::get<LambertMaterial>(&surface.m)) {
	//			Vec3 next_p = final_path.nodes[i + 1].ms.p;
	//			Vec3 omega_i = glm::normalize(next_p - surface.p);
	//			double brdf = glm::one_over_pi<double>();
	//			double cos_term = glm::max(glm::dot(surface.n, omega_i), 0.0); // マイナスがいるようだが、原因は不明
	//			coef *= lambert->albedo * brdf * cos_term;

	//			omega_o = -omega_i;
	//			origin = node.ms.p;
	//		} else if(auto refrac = boost::get<RefractionMaterial>(&surface.m)) {
	//			Vec3 next_p = final_path.nodes[i + 1].ms.p;
	//			Vec3 omega_i = glm::normalize(next_p - surface.p);
	//			omega_o = -omega_i;
	//			origin = node.ms.p;
	//		} else if (auto specular = boost::get<PerfectSpecularMaterial>(&surface.m)) {
	//			Vec3 next_p = final_path.nodes[i + 1].ms.p;
	//			Vec3 omega_i = glm::normalize(next_p - surface.p);
	//			omega_o = -omega_i;
	//			origin = node.ms.p;
	//		} else if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
	//			return coef * emissive->color / pdf;
	//		}
	//	}

	//	return Vec3();
	//}

	inline void step(AccumlationBuffer &buffer, const Scene &scene, int aa_sample) {
		double aa_sample_inverse = 1.0 / aa_sample;
		concurrency::parallel_for<int>(0, buffer._height, [&buffer, &scene, aa_sample, aa_sample_inverse](int y) {
		// for (int y = 0; y < buffer._height; ++y) {
			for (int x = 0; x < buffer._width; ++x) {
				int index = y * buffer._width + x;
				AccumlationBuffer::Pixel &pixel = buffer._data[index];

				Vec3 color;
				for (int aai = 0; aai < aa_sample; ++aai) {
					auto aa_offset = Vec2(
						pixel.engine.continuous() - 0.5,
						pixel.engine.continuous() - 0.5
					);

					/* ビュー空間 */
					auto ray_view = scene.camera.generate_ray(x + aa_offset.x, y + aa_offset.y, buffer._width, buffer._height);

					/* ワールド空間 */
					auto ray = scene.viewTransform.to_local_ray(ray_view);

					color += radiance(ray, scene, pixel.engine);
				}
				color *= aa_sample_inverse;

				// TODO 対症療法すぎるだろうか
				if (glm::all(glm::lessThan(color, Vec3(500.0)))) {
					pixel.color += color;
				}
			}
		});
		//}

		buffer._iteration += 1;
		buffer._ray_count += aa_sample;
	}
}

namespace lc {
	inline cinder::Surface32fRef to_surface(const AccumlationBuffer &buffer) {
		auto surface = cinder::Surface32f::create(buffer._width, buffer._height, false);
		double normalize_value = 1.0 / buffer._iteration;

		concurrency::parallel_for<int>(0, buffer._height, [&buffer, surface, normalize_value](int y) {
			float *lineHead = surface->getData(glm::ivec2(0, y));
			for (int x = 0; x < buffer._width; ++x) {
				int index = y * buffer._width + x;
				const AccumlationBuffer::Pixel &pixel = buffer._data[index];
				float *dstRGB = lineHead + x * 3;

				Vec3 color = pixel.color * normalize_value;

				for (int i = 0; i < 3; ++i) {
					dstRGB[i] = static_cast<float>(color[i]);
				}
			}
		});
		return surface;
	}


	// すこぶる微妙な気配
	inline cinder::Surface32fRef median_filter(cinder::Surface32fRef image) {
		int width = image->getWidth();
		int height = image->getHeight();
		auto surface = cinder::Surface32f::create(width, height, false);
		float *lineHeadDst = surface->getData();
		float *lineHeadSrc = image->getData();
		concurrency::parallel_for<int>(0, height, [lineHeadDst, lineHeadSrc, width, height](int y) {
			for (int x = 0; x < width; ++x) {
				int index = y * width + x;
				struct Pix {
					Pix() {}
					Pix(const float *c):color(c[0], c[1], c[2]), luminance(0.299f*c[0] + 0.587f*c[1] + 0.114f*c[2]){
					}
					glm::vec3 color;
					float luminance;
				};

				std::array<Pix, 5> p3x3;
				//p3x3[0] = Pix(lineHeadSrc + (std::max(y - 1, 0) * width + std::max(x - 1, 0)) * 3);
				p3x3[0] = Pix(lineHeadSrc + (std::max(y - 1, 0) * width + x) * 3);
				//p3x3[2] = Pix(lineHeadSrc + (std::max(y - 1, 0) * width + std::min(x + 1, width - 1)) * 3);

				p3x3[1] = Pix(lineHeadSrc + (y * width + std::max(x - 1, 0)) * 3);
				p3x3[2] = Pix(lineHeadSrc + (y * width + x) * 3);
				p3x3[3] = Pix(lineHeadSrc + (y * width + std::min(x + 1, width - 1)) * 3);

				//p3x3[6] = Pix(lineHeadSrc + (std::min(y + 1, height - 1) * width + std::max(x - 1, 0)) * 3);
				p3x3[4] = Pix(lineHeadSrc + (std::min(y + 1, height - 1) * width + x) * 3);
				//p3x3[8] = Pix(lineHeadSrc + (std::min(y + 1, height - 1) * width + std::min(x + 1, width - 1)) * 3);
				
				std::nth_element(p3x3.begin(), p3x3.begin() + 2, p3x3.end(), [](const Pix &a, const Pix &b) { 
					return a.luminance < b.luminance;
				});

				Pix p = p3x3[2];
				float *dstRGB = lineHeadDst + (y * width + x) * 3;
				for (int i = 0; i < 3; ++i) {
					dstRGB[i] = p.color[i];
				}
			}
		});
		return surface;
	}
}


using namespace ci;
using namespace ci::app;
using namespace std;



class RayTracerApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	void write_exr(const char *name, cinder::Surface32fRef surface) {
		auto dstPath = getAssetPath("") / name;
		writeImage(dstPath, *surface, ImageTarget::Options().quality(1.0f), "");
	}

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	lc::AccumlationBuffer *_buffer = nullptr;
	lc::Scene _scene;
	cinder::Surface32fRef _surface;
	gl::Texture2dRef _texture;
	gl::FboRef _fbo;
	gl::GlslProgRef _previewShader;

	double _renderTime = 0.0;

	bool _render = false;
	float _previewScale = 1.0f;
	float _previewGamma = 2.2f;
	bool _median = false;
};



void RayTracerApp::setup()
{
	_controlfp_s(NULL, _EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INEXACT, _MCW_EM);

	//double b = 0.0;
	//double a = 0.0;
	//double aaa = a / b;

	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 60.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 500.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(100.0f)).subdivisions(ivec2(10)), colorShader);

	//cinder::ObjLoader loader(loadAsset("bunny.obj"));
	//_mesh = cinder::TriMesh::create(loader);
	//_bvh.set_triangle(lc::to_triangles(_mesh));
	//_bvh.build();

	gl::Fbo::Format format = gl::Fbo::Format()
		.colorTexture()
		.disableDepth();
	_fbo = gl::Fbo::create(wide, wide, format);
	{
		gl::ScopedFramebuffer fb(_fbo);
		gl::ScopedViewport vp(ivec2(0), _fbo->getSize());
		gl::ScopedMatrices m;
		gl::clear(Color(0.25, 0.25, 0.25));
	}
	_previewShader = gl::GlslProg::create(gl::GlslProg::Format()
		.vertex(loadAsset("preview_shader/shader.vert"))
		.fragment(loadAsset("preview_shader/shader.frag")));

	_buffer = new lc::AccumlationBuffer(wide, wide);

	lc::Vec3 eye(0.0, 0.0, 60.0);
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };

	lc::Camera::Settings camera_settings;
	camera_settings.fovy = glm::radians(70.0);
	_scene.camera = lc::Camera(camera_settings);
	_scene.viewTransform = lc::Transform(glm::lookAt(eye, look_at, up));

	_scene.add(lc::ConelBoxObject(50.0));

	//auto spec = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(-10.0, -15, -10.0), 10.0),
	//	lc::PerfectSpecularMaterial()
	//);
	//_scene.add(spec);

	for (int i = 0; i < 5; ++i) {
		auto spec = lc::SphereObject(
			lc::Sphere(lc::Vec3(
				-20.0 + i * 10.0,
				-18.0,
				0.0), 5.0f),
			lc::CookTorranceMaterial(lc::Vec3(1.0), 0.1 + i * 0.2, 0.99 /*フレネル*/)
		);
		_scene.add(spec);
	}
	//auto spec = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(0.0, -10, 0.0), 10.0),
	//	lc::CookTorranceMaterial(lc::Vec3(1.0), 0.01, 0.01f /*フレネル*/)
	//);
	//_scene.add(spec);


	//auto spec = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(-10.0, -15, -10.0), 10.0),
	//	lc::CookTorranceMaterial(lc::Vec3(0.9), 0.01, 0.01f /*フレネル*/)
	//);
	//_scene.add(spec);

	//auto grass = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(10.0, -15, 10.0), 10.0),
	//	lc::RefractionMaterial(1.4)
	//);
	//_scene.add(grass);


	//cinder::ObjLoader loader(loadAsset("dragon.obj"));
	//auto mesh = cinder::TriMesh::create(loader);

	// TODO scaleバグ

	// auto mesh = cinder::TriMesh::create(cinder::geom::Sphere().radius(10.0));

	/*auto dragon = lc::TriangleMeshObject();
	dragon.bvh.set_triangle(lc::to_triangles(mesh));
	for (int i = 0; i < dragon.bvh._triangles.size(); ++i) {
		for (int j = 0; j < 3; ++j) {
			dragon.bvh._triangles[i].v[j] *= 30.0;
		}
	}
	dragon.bvh.build();
	lc::Mat4 dragonMat;
	dragonMat = glm::translate(dragonMat, lc::Vec3(0.0, -20.0, 0.0));
	dragon.transform = lc::Transform(dragonMat);
	dragon.material = lc::RefractionMaterial(1.4);
	_scene.objects.push_back(dragon);*/

	// でかいライト
	//{
	//	auto light = lc::DiscLight();
	//	light.disc = lc::make_disc(
	//		lc::Vec3(0.0, 24.0, 0.0),
	//		lc::Vec3(0.0, -1.0, 0.0),
	//		10.0
	//	);
	//	light.emissive = lc::EmissiveMaterial(lc::Vec3(10.0));
	//	_scene.add(light);
	//}

	{
		auto light = lc::DiscLight();
		light.disc = lc::make_disc(
			lc::Vec3(0.0, 24.0, 0.0),
			lc::Vec3(0.0, -1.0, 0.0),
			7
		);
		light.emissive = lc::EmissiveMaterial(lc::Vec3(10.0));
		_scene.add(light);
	}
	{
		auto light = lc::DiscLight();
		light.disc = lc::make_disc(
			lc::Vec3(20.0f, 10.0, 0.0),
			glm::normalize(lc::Vec3(-1.0, -1.0, 0.0)),
			5
		);
		light.emissive = lc::EmissiveMaterial(lc::Vec3(10.0));
		_scene.add(light);
	}
	_scene.finalize();

	// _scene.importances.push_back(lc::ImportantArea(spec.sphere));
	// 
	// _scene.lights.push_back(lc::ImportantArea(light.sphere, light.object_id));
	// _scene.importances.push_back(lc::ImportantArea(grass.sphere, grass.object_id));
}

void RayTracerApp::mouseDown(MouseEvent event)
{
}

void RayTracerApp::update()
{
}


void RayTracerApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	gl::ScopedDepth depth_test(true);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}

	lc::Path path;
	if (_render == false) {
		lc::draw_scene(_scene, wide, wide);

		auto ray_view = _scene.camera.generate_ray(_buffer->_width * 0.5, _buffer->_height * 0.5, _buffer->_width, _buffer->_height);

		gl::ScopedColor color(1.0, 0.5, 0.0);

		auto ray = _scene.viewTransform.to_local_ray(ray_view);

		
		//if (path.intersections.empty() == false) {
		//	gl::drawLine(ray.o, path.intersections[0].p);
		//	for (int i = 0; i < path.intersections.size() - 1; ++i) {
		//		gl::drawLine(path.intersections[i].p, path.intersections[i + 1].p);
		//	}
		//}
	}

	const int aa = 2;
	bool fbo_update = false;
	
	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::Text("samples: %d", _buffer->_iteration);
	ui::Text("time: %.2f s", _renderTime);
	ui::Text("rays per miliseconds: %.2f", aa * _buffer->_width * _buffer->_height * _buffer->_iteration * 0.001 / (_renderTime + 0.0001));
	
	ui::Checkbox("render", &_render);

	if (_render) {
		double beg = getElapsedSeconds();
		lc::step(*_buffer, _scene, aa);
		double duration = getElapsedSeconds() - beg;

		_renderTime += duration;

		if (_median) {
			_surface = lc::median_filter(lc::to_surface(*_buffer));
		} else {
			_surface = lc::to_surface(*_buffer);
		}
		_texture = gl::Texture2d::create(*_surface);

		fbo_update = true;
	}
	if (ui::SliderFloat("preview scale", &_previewScale, 0.0f, 5.0f)) {
		fbo_update = true;
	}
	if (ui::SliderFloat("preview gamma", &_previewGamma, 0.0f, 4.0f)) {
		fbo_update = true;
	}
	// ui::Checkbox("median", &_median);
	if (ui::Button("save")) {
		write_exr("image.exr", _surface);
	}
	
	
	if (fbo_update && _texture) {
		gl::ScopedFramebuffer fb(_fbo);
		gl::ScopedViewport vp(ivec2(0), _fbo->getSize());
		gl::ScopedMatrices m;
		gl::setMatricesWindow(_fbo->getSize());

		gl::ScopedGlslProg shaderScp(_previewShader);
		gl::ScopedTextureBind texBindScp(_texture);
		_previewShader->uniform("u_scale", _previewScale);
		_previewShader->uniform("u_gamma_correction", 1.0f / _previewGamma);
		gl::drawSolidRect(_texture->getBounds());
	}

	ui::Image(_fbo->getTexture2d(GL_COLOR_ATTACHMENT0), ImVec2(_fbo->getWidth(), _fbo->getHeight()));
}

CINDER_APP(RayTracerApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(true);
});
