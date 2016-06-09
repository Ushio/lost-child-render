#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include <ppl.h>

#define LC_USE_STD_FILESYSTEM

#include "collision_triangle.hpp"
#include "bvh.hpp"
#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "helper_cinder/draw_camera.hpp"
#include "helper_cinder/draw_scene.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "refraction.hpp"
#include "material.hpp"
#include "scene.hpp"

#include <stack>
#include <chrono>

#include <boost/format.hpp>
#include <boost/variant.hpp>

static const int wide = 256;

namespace lc {
	// typedef MersenneTwister EngineType;
	// typedef LCGs EngineType;
	typedef Xor EngineType;
	// typedef Xor128 EngineType;

	struct AccumlationBuffer {
		AccumlationBuffer(int width, int height, int random_skip = 50):_width(width), _height(height), _data(width * height){
			for (int y = 0; y < _height; ++y) {
				for (int x = 0; x < _width; ++x) {
					int index = y * _width + x;
					Pixel &pixel = _data[index];
					pixel.engine = EngineType(index);

					for (int i = 0; i < random_skip; ++i) {
						pixel.engine();
					}
				}
			}
		}
		struct Pixel {
			Vec3 color;
			EngineType engine;
		};

		int _width = 0;
		int _height = 0;
		std::vector<Pixel> _data;
		int _iteration = 0;
		int _ray_count = 0;
	};

	static const int kMaxDepth = 100;
	static const int kMinDepth = 3;
	static const Vec3 kReflectionBias(0.000001);

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

	inline Vec3 radiance(const Ray &ray, const Scene &scene, EngineType &engine) {
		Context context;

		Ray curr_ray = ray;
		Vec3 coef(1.0);
		
		// std::vector<Vec3> explicit_colors;

		struct Contribution {
			Vec3 color;
			double pdf = 0.0;
		};
		std::vector<Contribution> explicit_contributions;
		explicit_contributions.reserve(10);
		Contribution implicit_contribution;

		for (;;) {
			auto intersection = intersect(curr_ray, scene);
			if (!intersection) {
				break;
			}

			if (kMaxDepth < context.depth) {
				break;
			}

			auto surface = intersection->surface;

			double trace_p;
			if (context.depth < kMinDepth) {
				trace_p = 1.0;
			}
			else {
				if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
					trace_p = 1.0;
				}
				else {
					// 案外重要かも
					trace_p = glm::max(glm::max(context.color_weight.r, context.color_weight.g), context.color_weight.b) * glm::pow(0.9, context.diffusion);
					trace_p = glm::clamp(trace_p, 0.0, 1.0);
				}
			}

			trace_p = glm::max(trace_p, 0.01);

			if (trace_p <= generate_continuous(engine)) {
				// ロシアンルーレットによる終了
				break;
			}

			Vec3 omega_o = -curr_ray.d;
			if (auto lambert = boost::get<LambertMaterial>(&surface.m)) {
				double brdf = glm::one_over_pi<double>();

				HemisphereTransform hemisphereTransform(surface.n);

				//double indirect_p = 0.5 * glm::pow(0.5, context.diffusion);
				//bool is_next_direct = indirect_p < generate_continuous(engine);
				//if (is_next_direct) {
				//	if (boost::optional<Sample<Vec3>> sample_imp = sample_important_position(scene, surface.p, engine, context.diffusion)) {
				//		Vec3 omega_i = glm::normalize(sample_imp->value - surface.p);
				//		double pdf = sample_imp->pdf;

				//		if (0.0001 < glm::dot(omega_i, surface.n)) {
				//			Ray direct_ray(glm::fma(omega_i, kReflectionBias, surface.p), omega_i);
				//			if (auto direct_intersection = intersect(direct_ray, scene)) {
				//				if (is_important(scene, direct_intersection->object_id)) {
				//					double cos_term = glm::dot(surface.n, omega_i);
				//					coef *= lambert->albedo * brdf * cos_term / pdf / trace_p;
				//					curr_ray = direct_ray;
				//					context.step().diffuse(1.0).albedo(lambert->albedo);
				//					intersection = direct_intersection;
				//					skip_intersection = true;
				//					continue;
				//				}
				//			}
				//		}
				//	}
				//}

				Ray next_ray;
				double implicit_pdf;
				//if (scene.importances.empty() || generate_continuous(engine) < 0.5) {
					Sample<Vec3> cos_sample = generate_cosine_weight_hemisphere(engine);
					Vec3 direction = hemisphereTransform.transform(cos_sample.value);
					implicit_pdf = cos_sample.pdf;
					next_ray = Ray(glm::fma(direction, kReflectionBias, surface.p), direction);
				//}
				//else {
				//	auto s = important_object_sample(scene, surface.p, engine);
				//	next_ray = s.ray;
				//	implicit_pdf = s.pdf;
				//}
				Vec3 omega_i = next_ray.d;

				double cos_term = glm::dot(surface.n, omega_i);
				// coef *= lambert->albedo * brdf * cos_term / pdf_implicit / trace_p;
				coef *= lambert->albedo * brdf * cos_term / trace_p;

				auto direct = direct_sample(scene, surface.p, engine);
				if (auto direct_intersection = intersect(direct.ray, scene)) {
					if (auto emissive = boost::get<EmissiveMaterial>(&direct_intersection->surface.m)) {
						Contribution c;
						c.color = coef * emissive->color / direct.pdf;
						c.pdf = direct.pdf;
						explicit_contributions.push_back(c);
					}
				}
				if (implicit_contribution.pdf < glm::epsilon<double>()) {
					implicit_contribution.pdf = implicit_pdf;
				} else {
					implicit_contribution.pdf *= implicit_pdf;
				}

				curr_ray = Ray(glm::fma(omega_i, kReflectionBias, surface.p), omega_i);
				context.step().diffuse(1.0).albedo(lambert->albedo);

				continue;
			} else if(auto refrac = boost::get<RefractionMaterial>(&surface.m)) {
				double eta = surface.isback ? refrac->ior / 1.0 : 1.0 / refrac->ior;

				auto fresnel = [](double costheta, double f0) {
					double f = f0 + (1.0 - f0) * glm::pow(1.0 - costheta, 5.0);
					return f;
				};
				double fresnel_value = fresnel(dot(omega_o, surface.n), 0.02);

				if (fresnel_value < generate_continuous(engine)) {
					auto omega_i_refract = refraction(-omega_o, surface.n, eta);

					curr_ray = Ray(glm::fma(omega_i_refract, kReflectionBias, surface.p), omega_i_refract);
					coef /= trace_p;
					context.step();
					continue;
				}
				else {
					auto omega_i_reflect = glm::reflect(-omega_o, surface.n);
					curr_ray = Ray(glm::fma(omega_i_reflect, kReflectionBias, surface.p), omega_i_reflect);
					coef /= trace_p;
					context.step();
					continue;
				}
			}
			else if (auto specular = boost::get<PerfectSpecularMaterial>(&surface.m)) {
				auto omega_i_reflect = glm::reflect(-omega_o, surface.n);
				curr_ray = Ray(glm::fma(omega_i_reflect, kReflectionBias, surface.p), omega_i_reflect);
				coef /= trace_p;
				context.step();
				continue;
			} else if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
				implicit_contribution.color = coef * emissive->color / implicit_contribution.pdf / trace_p;

				if (context.diffusion < glm::epsilon<double>()) {
					return emissive->color;
				}
				break;
				// TODO
				// return coef * emissive->color;
			}
		}

		//printf("im: %.2f, ex: ", implicit_contribution.pdf);
		//Vec3 color;
		//for (int i = 0; i < explicit_contributions.size(); ++i) {
		//	color += explicit_contributions[i].color;
		//	printf("%.2f, ", explicit_contributions[i].pdf);
		//}
		//printf("\n");
		// return glm::mix(color, implicit_contribution.color, 0.5);
		if (explicit_contributions.empty()) {
			return implicit_contribution.color;
		}
		if (implicit_contribution.pdf < glm::epsilon<double>()) {
			return Vec3();
		}

		Vec3 imp_color = implicit_contribution.color / explicit_contributions.size();
		Vec3 color;
		for (int i = 0; i < explicit_contributions.size(); ++i) {
			double ws = explicit_contributions[i].pdf + implicit_contribution.pdf;
			Vec3 value = 
				explicit_contributions[i].color * explicit_contributions[i].pdf
				+
				imp_color * implicit_contribution.pdf;
			color += value / ws;
		}
		return color;
	}
	//inline Vec3 radiance(const Ray &ray, const Scene &scene, EngineType &engine, const Context &context = Context()) {
	//	auto intersection = intersect(ray, scene);
	//	if (!intersection) {
	//		return Vec3(0.0);
	//	}

	//	// ダイレクトサンプリング時、
	//	// 目的のオブジェクトにたどり着けなかったら早々にあきらめる
	//	if (context.is_direct_sample) {
	//		auto intersected_id = intersection->object_id;
	//		bool succeeded_sample = false;
	//		for (const ImportantArea &area : boost::join(scene.lights, scene.importances)) {
	//			if (area.object_id == intersected_id) {
	//				succeeded_sample = true;
	//				break;
	//			}
	//		}
	//		if (succeeded_sample == false) {
	//			return Vec3(0.0);
	//		}
	//	}

	//	auto surface = intersection->surface;

	//	// バイアスが発生するが堅実
	//	if (kMaxDepth < context.depth) {
	//		return Vec3(0.0);
	//	}

	//	// direct
	//	// 0.0f => 失敗率100%
	//	// 1.0f => 失敗率0%
	//	double trace_p;
	//	if (context.depth < kMinDepth) {
	//		trace_p = 1.0;
	//	}
	//	else if (kMaxDepth < context.depth) {
	//		return Vec3(0.0);
	//	}
	//	else {
	//		if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
	//			trace_p = 1.0;
	//		}
	//		else {
	//			// 案外重要かも
	//			trace_p = glm::max(glm::max(context.color_weight.r, context.color_weight.g), context.color_weight.b) * glm::pow(0.9, context.diffusion);
	//			trace_p = glm::clamp(trace_p, 0.0, 1.0);
	//		}
	//	}

	//	trace_p = glm::max(trace_p, 0.01);

	//	if (trace_p <= generate_continuous(engine)) {
	//		// ロシアンルーレットによる終了
	//		return Vec3();
	//	}

	//	Vec3 omega_o = -ray.d;
	//	if (auto lambert = boost::get<LambertMaterial>(&surface.m)) {

	//		// value * 0.25 * x = value * 0.5
	//		// 0.25 * x = 0.5
	//		// x = 0.5 / 0.25
	//		// 
	//		// double indirect_p = glm::max(0.95 * glm::pow(0.25, context.diffusion), 0.2);
	//		double indirect_p = 0.5 * glm::pow(0.5, context.diffusion);
	//		bool is_next_direct = indirect_p < generate_continuous(engine);
	//		// double sample_pdf = is_next_direct ? (1.0 - indirect_p) / 0.5 : indirect_p / 0.5;
	//		// double sample_pdf = 1.0;
	//		HemisphereTransform hemisphereTransform(surface.n);


	//		double pdf = 0.0;
	//		Vec3 omega_i;
	//		Vec3 L;
	//		bool succeeded_direct_sample = false;

	//		// ダイレクトサンプリング
	//		if (is_next_direct) {
	//			// TODO depthなにに使ってるんだっけ？
	//			if (boost::optional<Sample<Vec3>> sample_imp = sample_important_position(scene, surface.p, engine, context.depth)) {
	//				omega_i = glm::normalize(sample_imp->value - surface.p);
	//				pdf = sample_imp->pdf;

	//				if (0.0001 < glm::dot(omega_i, surface.n)) {
	//					Ray next_ray(glm::fma(omega_i, kReflectionBias, surface.p), omega_i);
	//					L = radiance(next_ray, scene, engine, context.step().direct().diffuse(1.0).albedo(lambert->albedo)) / trace_p;
	//					if (glm::any(glm::greaterThan(L, Vec3(0.0)))) {
	//						succeeded_direct_sample = true;
	//					}
	//				}
	//			}
	//		}

	//		// ダイレクトサンプリングがなされなかったとき
	//		if (succeeded_direct_sample == false) {
	//			// コサイン重点サンプリング
	//			Sample<Vec3> cos_sample = generate_cosine_weight_hemisphere(engine);
	//			omega_i = hemisphereTransform.transform(cos_sample.value);
	//			pdf = cos_sample.pdf;

	//			// 一様サンプリング
	//			// omega_i = hemisphereTransform.transform(generate_on_hemisphere(engine));
	//			// pdf = glm::one_over_two_pi<double>();

	//			Ray next_ray(glm::fma(omega_i, kReflectionBias, surface.p), omega_i);
	//			L = radiance(next_ray, scene, engine, context.step().diffuse(1.0).albedo(lambert->albedo)) / trace_p;
	//		}

	//		double brdf = glm::one_over_pi<double>();
	//		double cos_term = glm::dot(surface.n, omega_i);
	//		return lambert->albedo * brdf * cos_term * L / pdf;
	//	} else if (auto refrac = boost::get<RefractionMaterial>(&surface.m)) {
	//		double eta = surface.isback ? refrac->ior / 1.0 : 1.0 / refrac->ior;
	//		
	//		auto fresnel = [](double costheta, double f0) {
	//			double f = f0 + (1.0 - f0) * glm::pow(1.0 - costheta, 5.0);
	//			return f;
	//		};
	//		double fresnel_value = fresnel(dot(omega_o, surface.n), 0.02);

	//		if (fresnel_value < generate_continuous(engine)) {
	//			auto omega_i_refract = refraction(-omega_o, surface.n, eta);

	//			Ray refract_ray;
	//			refract_ray.o = glm::fma(omega_i_refract, kReflectionBias, surface.p);
	//			refract_ray.d = omega_i_refract;
	//			return radiance(refract_ray, scene, engine, context.step()) / trace_p;
	//		}
	//		else {
	//			auto omega_i_reflect = glm::reflect(-omega_o, surface.n);

	//			Ray reflect_ray;
	//			reflect_ray.o = glm::fma(omega_i_reflect, kReflectionBias, surface.p);
	//			reflect_ray.d = omega_i_reflect;
	//			return radiance(reflect_ray, scene, engine, context.step()) / trace_p;
	//		}
	//	} else if (auto specular = boost::get<PerfectSpecularMaterial>(&surface.m)) {
	//		auto omega_i_reflect = glm::reflect(-omega_o, surface.n);
	//		Ray reflect_ray;
	//		reflect_ray.o = glm::fma(omega_i_reflect, kReflectionBias, surface.p);
	//		reflect_ray.d = omega_i_reflect;
	//		return radiance(reflect_ray, scene, engine, context.step()) / trace_p;
	//	} else if (auto emissive = boost::get<EmissiveMaterial>(&surface.m)) {
	//		return emissive->color;
	//	}
	//	return Vec3(0.1);
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
						generate_continuous(pixel.engine) - 0.5,
						generate_continuous(pixel.engine) - 0.5
					);

					/* ビュー空間 */
					auto ray_view = scene.camera.generate_ray(x + aa_offset.x, y + aa_offset.y, buffer._width, buffer._height);

					/* ワールド空間 */
					auto ray = scene.viewTransform.to_local_ray(ray_view);

					color += radiance(ray, scene, pixel.engine);
				}
				color *= aa_sample_inverse;

				// TODO 対症療法すぎるだろうか
				if (glm::all(glm::lessThan(color, Vec3(200.0)))) {
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

	_scene.objects.push_back(lc::ConelBoxObject(50.0));
	//_scene.objects.push_back(lc::SphereObject(
	//	lc::Sphere(lc::Vec3(0.0, -15, 0.0), 10.0),
	//	lc::LambertMaterial(lc::Vec3(1.0))
	//));

	// テスト
	//_scene.objects.push_back(lc::SphereObject(
	//	lc::Sphere(lc::Vec3(0.0, 0.0, 0.0), 5.0),
	//	lc::LambertMaterial(lc::Vec3(0.75))
	//));

	auto spec = lc::SphereObject(
		lc::Sphere(lc::Vec3(-10.0, -15, -10.0), 10.0),
		lc::PerfectSpecularMaterial()
	);
	_scene.objects.push_back(spec);

	auto grass = lc::SphereObject(
		lc::Sphere(lc::Vec3(10.0, -15, 10.0), 10.0),
		lc::RefractionMaterial(1.4)
	);
	_scene.objects.push_back(grass);


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

	auto light = lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0, 20.0, 0.0), 5.0),
		lc::EmissiveMaterial(lc::Vec3(10.0))
	);
	_scene.objects.push_back(light);

	// _scene.importances.push_back(lc::ImportantArea(spec.sphere));
	// 
	_scene.lights.push_back(lc::ImportantArea(light.sphere, light.object_id));
	_scene.importances.push_back(lc::ImportantArea(grass.sphere, grass.object_id));
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

	if (_render == false) {
		lc::draw_scene(_scene, wide, wide);
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
		for (int i = 0; i < 5; ++i) {
			lc::step(*_buffer, _scene, aa);
		}
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
	ui::Checkbox("median", &_median);
	
	
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
