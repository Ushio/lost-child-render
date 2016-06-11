﻿#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include <ppl.h>

#define LC_USE_STD_FILESYSTEM

#include "constants.hpp"
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

	struct Path {
		struct Node {
			Vec3 omega_i;
			Vec3 omega_o;

			// ここまでのパスを活用する場合の係数全部入り
			// ここ自身のBRDFなどは考慮に入れられていない
			// なぜなら結合の際に方向が変わってしまうため
			Vec3 coef = Vec3(1.0);

			MicroSurface surface;
		};
		std::vector<Node> nodes;
	};
	
	inline bool visible(const Scene &scene, const Vec3 &p0, const Vec3 &p1) {
		Vec3 d = p1 - p0;
		double lengthSquared = glm::length2(d);
		if (lengthSquared < 0.0001) {
			return true;
		}
		if (auto intersection = intersect(Ray(p0, d / glm::sqrt(lengthSquared)), scene)) {
			return glm::distance2(intersection->surface.p, p1) < glm::epsilon<double>();
		}
		return false;
	}

	inline Path trace(const Ray &ray, const Scene &scene, EngineType &engine, bool is_light) {
		Ray curr_ray = ray;

		Path path;
		Vec3 coef(1.0);
		int diffusion_count = 0;
		for (int i = 0; i < 10; ++i) {
			// 数字を小さくすると早めに終了するようになる
			if (glm::pow(0.5, diffusion_count) < generate_continuous(engine)) {
				break;
			}

			auto intersection = intersect(curr_ray, scene);
			if (!intersection) {
				break;
			}
			auto surface = intersection->surface;

			Vec3 omega_o = -curr_ray.d;
			if (auto lambert = boost::get<LambertMaterial>(&surface.m)) {
				diffusion_count++;

				HemisphereTransform hemisphereTransform(surface.n);

				// BRDF
				Sample<Vec3> cos_sample = generate_cosine_weight_hemisphere(engine);
				Vec3 omega_i = hemisphereTransform.transform(cos_sample.value);
				double pdf = cos_sample.pdf;
				Vec3 omega_o = -curr_ray.d;

				if (is_light) {
					std::swap(omega_i, omega_o);
				}

				double brdf = glm::one_over_pi<double>();
				double cos_term = glm::max(glm::dot(surface.n, omega_i), 0.0); // マイナスがいるようだが、原因は不明
				Vec3 this_coef = lambert->albedo * brdf * cos_term / pdf;

				Path::Node node;
				node.coef = coef;
				node.surface = surface;
				node.omega_i = omega_i;
				node.omega_o = omega_o;
				path.nodes.push_back(node);

				coef *= this_coef;

				curr_ray = Ray(glm::fma(omega_o, kReflectionBias, surface.p), omega_i);
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
				//if (is_light) {
				//	break;
				//}
				//Path::Node node;
				//node.omega_o = -curr_ray.d;
				//node.surface = surface;
				//path.nodes.push_back(node);
				//break;
				break;
			}
		}
		return path;
	}

	inline Vec3 connect(const Path &camera_path, const Path &light_path, int camera_nodes, int light_nodes) {



	}
	
	inline double weight(int ci, int li) {
		return 1.0 / (double(ci) + double(li) + 1.0);
	}

	inline Vec3 radiance(const Ray &camera_ray, const Scene &scene, EngineType &engine) {
		// 通常のパストレーシング
		Path camera_path = trace(camera_ray, scene, engine, false);

		// ライトからのレイを生成する
		OnLight light = on_light(scene, engine);
		
		Ray light_ray = Ray(light.p, generate_on_sphere(engine));

		// ライトトレーシング
		Path light_path = trace(light_ray, scene, engine, true);

		// そもそもカメラレイが衝突していない
		if (camera_path.nodes.empty()) {
			return Vec3();
		}

		// light.emissive.color

		// 
		//struct Contribution {
		//	Vec3 color;
		//	double w = 
		//};

		Vec3 color;
		double weight_all = 0.0;

		for (int ci = 0; ci < camera_path.nodes.size(); ++ci) {
			// NEE
			Path::Node camera_node = camera_path.nodes[ci];
			if (auto lambert = boost::get<LambertMaterial>(&camera_node.surface.m)) {
				auto direct = direct_sample(scene, camera_node.surface.p, engine);
				if (auto direct_intersect = intersect(direct.ray, scene)) {
					if (auto emissive = boost::get<EmissiveMaterial>(&direct_intersect->surface.m)) {
						Vec3 omega_i = glm::normalize(light.p - camera_node.surface.p);
						double brdf = glm::one_over_pi<double>();
						double cos_term = glm::max(glm::dot(camera_node.surface.n, omega_i), 0.0); // マイナスがいるようだが、原因は不明
						Vec3 this_coef = lambert->albedo * brdf * cos_term;

						double w = weight(ci, 0);
						color += this_coef * emissive->color * camera_node.coef / direct.pdf * w;
						weight_all += w;
					}
				}
			}
			
			for (int li = 0; li < light_path.nodes.size(); ++li) {

			}
		}

		return weight_all <= glm::epsilon<double>() ? Vec3() : color / weight_all;
	}
	//inline Vec3 radiance(const Ray &camera_ray, const Scene &scene, EngineType &engine) {
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

	static auto light = lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0, 20.0, 0.0), 5.0),
		lc::EmissiveMaterial(lc::Vec3(10.0))
	);
	_scene.objects.push_back(light);

	_scene.lights.push_back(&light);

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

	static int step = 0;
	lc::Xor engine(10);

	for (int i = 0; i < step; ++i) {
		engine();
	}

	lc::Path path;
	if (_render == false) {
		lc::draw_scene(_scene, wide, wide);

		auto ray_view = _scene.camera.generate_ray(_buffer->_width * 0.5, _buffer->_height * 0.5, _buffer->_width, _buffer->_height);

		gl::ScopedColor color(1.0, 0.5, 0.0);

		auto ray = _scene.viewTransform.to_local_ray(ray_view);
		// path = lc::trace(ray, _scene, engine);

		
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
	
	ui::SliderInt("step [debug]", &step, 0, 100);
	
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
