#pragma once

#include "parallel_for.hpp"

#include "constants.hpp"
#include "collision_triangle.hpp"
#include "bvh.hpp"
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
#include "fixed_vector.hpp"

namespace lc {
	struct AccumlationBuffer {
		AccumlationBuffer(int width, int height, int random_skip = 50) :_width(width), _height(height), _data(width * height) {
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
		fixed_vector<Node, kMaxDepth> nodes;
	};

	inline Path path_trace(const Ray &ray, const Scene &scene, DefaultEngine &engine) {
		Ray curr_ray = ray;
		Path path;

		Vec3 coef(1.0);
		double pdf = 1.0;
		double diffusion_count = 0;
		// Vec3 diffusion(1.0);

		double max_diffusion_count = 4.0;
		// path.nodes.reserve(max_trace);

		for (int i = 0; i < kMaxDepth && diffusion_count < max_diffusion_count; ++i) {
			auto intersection = intersect(curr_ray, scene);
			if (!intersection) {
				break;
			}
			auto surface = *intersection;

			Vec3 omega_o = -curr_ray.d;
			if (auto lambert = boost::get<LambertMaterial>(&surface.m)) {
				diffusion_count += 1.0;

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
			}
			else if (auto cook = boost::get<CookTorranceMaterial>(&surface.m)) {
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
				Vec3 albedo;
				if (engine.continuous() < f) {
					double g = G(omega_i, omega_o, ggx_sample.value.h, n, cook->roughness);
					double d = ggx_d(glm::dot(ggx_sample.value.h, n), cook->roughness);
					brdf = d * g / glm::max(4.0 * glm::dot(omega_o, n) * cos_term, kEPS);

					albedo = cook->albedo_specular;
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

					albedo = cook->albedo_diffuse;
				}

				Path::Node node;
				node.coef = coef;
				node.pdf = pdf;
				node.surface = surface;
				node.omega_i = omega_i;
				node.omega_o = omega_o;
				path.nodes.push_back(node);

				Vec3 this_coef = albedo * brdf * cos_term;

				coef *= this_coef;
				pdf *= this_pdf;

				curr_ray = Ray(glm::fma(omega_o, kReflectionBias, surface.p), omega_i);
				continue;
			}
			else if (auto refrac = boost::get<RefractionMaterial>(&surface.m)) {
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
			}
			else if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
				Path::Node node;
				node.coef = coef;
				node.pdf = pdf;
				// node.omega_i = 存在しない
				node.omega_o = -curr_ray.d;
				node.surface = surface;
				path.nodes.push_back(node);

				break;
			}
		}
		return path;
	}

	inline Vec3 radiance(const Ray &camera_ray, const Scene &scene, DefaultEngine &engine) {
		// 通常のパストレーシング
		Path camera_path = path_trace(camera_ray, scene, engine);

		// そもそもカメラレイが衝突していない
		if (camera_path.nodes.empty()) {
			return Vec3();
		}

		// そもそも最初がライトだった特殊ケース
		if (auto emissive = boost::get<EmissiveMaterial>(&camera_path.nodes[0].surface.m)) {
			return emissive->color;
		}

		Sample<Vec3> implicit_contribution;
		fixed_vector<Sample<Vec3>, kMaxDepth> explicit_contributions;

		for (int ci = 0; ci < camera_path.nodes.size(); ++ci) {
			bool is_term = ci + 1 == camera_path.nodes.size();

			Path::Node camera_node = camera_path.nodes[ci];
			if (auto lambert = boost::get<LambertMaterial>(&camera_node.surface.m)) {
				auto sample = direct_light_sample(scene, camera_node.surface.p, engine);
				// TODO emissiveが0ならやらなくていい？
				if (is_visible(sample->ray, scene, sample->tmin - kEPS)) {
					auto emissive = sample->onLight.emissive;
					double pdf = camera_node.pdf * sample.pdf;
					Vec3 omega_i = sample->ray.d;
					double brdf = glm::one_over_pi<double>();
					double cos_term = glm::max(glm::dot(camera_node.surface.n, omega_i), 0.0);
					Vec3 this_coef = lambert->albedo * brdf * cos_term;
					Sample<Vec3> contrib;
					contrib.value = this_coef * emissive.color * camera_node.coef / glm::max(pdf, kEPS);
					contrib.pdf = pdf;
					explicit_contributions.push_back(contrib);
				}
			}
			else if (auto cook = boost::get<CookTorranceMaterial>(&camera_node.surface.m)) {
				auto sample = direct_light_sample(scene, camera_node.surface.p, engine);

				auto emissive = sample->onLight.emissive;
				double pdf = camera_node.pdf * sample.pdf;

				Vec3 n = camera_node.surface.n;
				Vec3 omega_o = camera_node.omega_o;
				Vec3 omega_i = sample->ray.d;
				double cos_term = glm::max(glm::dot(n, omega_i), 0.0);
				double f = lc::fresnel(cos_term, cook->fesnel_coef);

				double brdf = 0.0;
				Vec3 albedo;
				if (engine.continuous() < f) {
					Vec3 h = glm::normalize(omega_i + omega_o);
					double g = G(omega_i, omega_o, h, n, cook->roughness);
					double d = ggx_d(glm::dot(h, n), cook->roughness);
					brdf = d * g / glm::max(4.0 * glm::dot(omega_o, n) * cos_term, kEPS);
					albedo = cook->albedo_specular;
				}
				else {
					brdf = glm::one_over_pi<double>();
					albedo = cook->albedo_diffuse;
				}

				Vec3 this_coef = albedo * brdf * cos_term;

				double max_coef = glm::max(glm::max(this_coef.r, this_coef.g), this_coef.b);
				double nee_probability = glm::clamp(max_coef * 1.5, 0.01, 1.0);
				if (engine.continuous() < nee_probability) {
					if (is_visible(sample->ray, scene, sample->tmin - kEPS)) {
						pdf *= nee_probability;

						Sample<Vec3> contrib;
						contrib.value = this_coef * emissive.color * camera_node.coef / glm::max(pdf, kEPS);
						contrib.pdf = pdf;
						explicit_contributions.push_back(contrib);
					}
				}
			}
			if (is_term) {
				if (auto emissive = boost::get<EmissiveMaterial>(&camera_node.surface.m)) {
					implicit_contribution.value = emissive->color * camera_node.coef / glm::max(camera_node.pdf, kEPS);
					implicit_contribution.pdf = camera_node.pdf;
				}
			}
		}

		// ビジュアライズ
		/*
		implicit_contribution.value = Vec3(0.4, 0.0, 0.0);
		for (int i = 0; i < explicit_contributions.size(); ++i) {
			explicit_contributions[i].value = Vec3(0.0, 0.4, 0.0);
		}
		*/

		// 片方の戦略しか使えない場合に対応
		if (explicit_contributions.empty()) {
			return *implicit_contribution;
		}
		if (implicit_contribution.pdf < glm::epsilon<double>()) {
			Vec3 color;
			for (int i = 0; i < explicit_contributions.size(); ++i) {
				color += *explicit_contributions[i];
			}
			return color;
		}

		// MISによるマージ
		Vec3 color;

		// 平均
		auto mu = [](const Vec3 v) {
			return (v.x + v.y + v.z) * (1.0 / 3.0);
		};

		double implicit_weight = glm::pow(implicit_contribution.pdf, 2.0);
		Vec3 implicit = implicit_contribution.value * implicit_weight / explicit_contributions.size();

		for (int i = 0; i < explicit_contributions.size(); ++i) {
			auto explicit_contribution = explicit_contributions[i];
			double explicit_weight = glm::pow(explicit_contribution.pdf * explicit_contributions.size(), 2.0);

			Vec3 sum = explicit_contribution.value * explicit_weight + implicit;
			double weight_sum = implicit_weight + explicit_weight;
			color += 0.0 < weight_sum ? sum / weight_sum : Vec3();
		}

		return color;
	}

	inline void step(AccumlationBuffer &buffer, const Scene &scene, int aa_sample) {
		double aa_sample_inverse = 1.0 / aa_sample;
		// concurrency::parallel_for<int>(0, buffer._height, [&buffer, &scene, aa_sample, aa_sample_inverse](int y) {
		parallel_for(buffer._height, [&buffer, &scene, aa_sample, aa_sample_inverse] (int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
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
			}
		});

		buffer._iteration += 1;
		buffer._ray_count += aa_sample;
	}
}