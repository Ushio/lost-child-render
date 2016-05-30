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
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
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
	};

	static const int kMaxDepth = 100;
	static const int kMinDepth = 3;
	static const double kReflectionBias = 0.0001;

	inline Vec3 radiance(const Ray &ray, const Scene &scene, EngineType &engine, const Vec3 &importance, int depth) {
		auto intersection = intersect(ray, scene);
		if (!intersection) {
			return Vec3(0.0);
		}

		// 0.0f => 失敗率100%
		// 1.0f => 失敗率0%
		double trace_p;
		if (depth < kMinDepth) {
			trace_p = 1.0;
		}
		else if (kMaxDepth < depth) {
			return Vec3(0.0);
		}
		else {
			if (auto emissive = boost::get<EmissiveMaterial>(&intersection->m)) {
				trace_p = 1.0;
			}
			else {
				// 案外重要かも
				trace_p = glm::max(glm::max(importance.r, importance.g), importance.b) * 0.9;
				trace_p = glm::clamp(trace_p, 0.0, 1.0);
			}
		}

		trace_p = glm::max(trace_p, 0.01);

		if (trace_p <= generate_continuous(engine)) {
			// ロシアンルーレットによる終了
			return Vec3();
		}

		Vec3 omega_o = -ray.d;

		if (auto lambert = boost::get<LambertMaterial>(&intersection->m)) {
			double pdf = 0.0;
			Vec3 omega_i;

			//double x = depth;
			//double a = -2.0;
			//double b = 4.0;
			//double direct_p = 1.0 / (1.0 + glm::exp(-a * (x - b)));
			//double indirect_p = 1.0 - direct_p;
			// double indirect = depth < 3 ? 1.0 : 0.0;
			if (generate_continuous(engine) < 0.5) {
				Sample<Vec3> cos_sample = generate_cosine_weight_hemisphere(engine);

				// 半球定義
				Vec3 yaxis = intersection->n;
				Vec3 xaxis;
				Vec3 zaxis;
				if (0.999 < glm::abs(yaxis.z)) {
					xaxis = glm::normalize(glm::cross(Vec3(0.0, -1.0, 0.0), yaxis));
				}
				else {
					xaxis = glm::normalize(glm::cross(Vec3(0.0, 0.0, 1.0), yaxis));
				}
				zaxis = glm::cross(xaxis, yaxis);

				omega_i = cos_sample.value.x * xaxis + cos_sample.value.y * yaxis + cos_sample.value.z * zaxis;
				pdf = cos_sample.pdf;
			} else {
				Sample<Vec3> sample_imp = sample_important_position(scene, intersection->p, engine);
				omega_i = glm::normalize(sample_imp.value - intersection->p);
				pdf = sample_imp.pdf;
				if (glm::dot(omega_i, intersection->n) <= 0.0001) {
					Sample<Vec3> cos_sample = generate_cosine_weight_hemisphere(engine);

					// 半球定義
					Vec3 yaxis = intersection->n;
					Vec3 xaxis;
					Vec3 zaxis;
					if (0.999 < glm::abs(yaxis.z)) {
						xaxis = glm::normalize(glm::cross(Vec3(0.0, -1.0, 0.0), yaxis));
					}
					else {
						xaxis = glm::normalize(glm::cross(Vec3(0.0, 0.0, 1.0), yaxis));
					}
					zaxis = glm::cross(xaxis, yaxis);

					omega_i = cos_sample.value.x * xaxis + cos_sample.value.y * yaxis + cos_sample.value.z * zaxis;
					pdf = cos_sample.pdf;
				}
			}

			Ray new_ray;
			new_ray.o = intersection->p + omega_i * kReflectionBias;
			new_ray.d = omega_i;

			double cos_term = glm::dot(intersection->n, omega_i);
			double brdf = glm::one_over_pi<double>();
			Vec3 L = radiance(new_ray, scene, engine, lambert->albedo, depth + 1) / trace_p;
			return lambert->albedo * brdf * cos_term * L / pdf;
		}
		if (auto refrac = boost::get<RefractionMaterial>(&intersection->m)) {
			double eta = intersection->isback ? refrac->ior / 1.0 : 1.0 / refrac->ior;
			auto omega_i_refract = refraction(-omega_o, intersection->n, eta);
			auto omega_i_reflect = glm::reflect(-omega_o, intersection->n);

			auto fresnel = [](double costheta, double f0) {
				double f = f0 + (1.0 - f0) * glm::pow(1.0 - costheta, 5.0);
				return f;
			};
			double fresnel_value = fresnel(dot(omega_o, intersection->n), 0.02);

			Ray refract_ray;
			refract_ray.o = intersection->p + omega_i_refract * kReflectionBias;
			refract_ray.d = omega_i_refract;

			Ray reflect_ray;
			reflect_ray.o = intersection->p + omega_i_reflect * kReflectionBias;
			reflect_ray.d = omega_i_reflect;

			if (depth == 0) {
				auto refract_color = radiance(refract_ray, scene, engine, importance, depth + 1);
				auto reflect_color = radiance(reflect_ray, scene, engine, importance, depth + 1);
				return mix(refract_color, reflect_color, fresnel_value) / trace_p;
			}
			else {
				if (fresnel_value < generate_continuous(engine)) {
					return radiance(refract_ray, scene, engine, importance, depth + 1) / trace_p;
				}
				else {
					return radiance(reflect_ray, scene, engine, importance, depth + 1) / trace_p;
				}
			}
		}
		if (auto specular = boost::get<PerfectSpecularMaterial>(&intersection->m)) {
			auto omega_i_reflect = glm::reflect(-omega_o, intersection->n);
			Ray reflect_ray;
			reflect_ray.o = intersection->p + omega_i_reflect * kReflectionBias;
			reflect_ray.d = omega_i_reflect;
			return radiance(reflect_ray, scene, engine, importance, depth + 1) / trace_p;
		}
		if (auto emissive = boost::get<EmissiveMaterial>(&intersection->m)) {
			return emissive->color;
		}
		return Vec3(0.1);
	}

	inline void step(AccumlationBuffer &buffer, const Scene &scene) {
		concurrency::parallel_for<int>(0, buffer._height, [&buffer, &scene](int y) {
			for (int x = 0; x < buffer._width; ++x) {
				int index = y * buffer._width + x;
				AccumlationBuffer::Pixel &pixel = buffer._data[index];

				/* ビュー空間 */
				auto ray_view = scene.camera.generate_ray(x, y, buffer._width, buffer._height);

				/* ワールド空間 */
				auto ray = scene.viewTransform.to_local_ray(ray_view);

				auto color = radiance(ray, scene, pixel.engine, Vec3(1.0), 0);
				pixel.color += color;
			}
		});

		buffer._iteration += 1;
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

	auto light = lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0, 20.0, 0.0), 5.0),
		lc::EmissiveMaterial(lc::Vec3(10.0))
	);
	_scene.objects.push_back(light);

	// _scene.importances.push_back(lc::ImportantArea(spec.sphere));
	// _scene.importances.push_back(lc::ImportantArea(grass.sphere));
	_scene.importances.push_back(lc::ImportantArea(light.sphere));
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

	bool fbo_update = false;
	
	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::Text("samples: %d", _buffer->_iteration);
	ui::Text("time: %.2f s", _renderTime);
	ui::Text("rays per miliseconds: %.2f", _buffer->_width * _buffer->_height * _buffer->_iteration * 0.001 / (_renderTime + 0.0001));
	ui::Checkbox("render", &_render);

	if (_render) {
		double beg = getElapsedSeconds();
		for (int i = 0; i < 5; ++i) {
			lc::step(*_buffer, _scene);
		}
		double duration = getElapsedSeconds() - beg;

		_renderTime += duration;

		_surface = lc::to_surface(*_buffer);
		_texture = gl::Texture2d::create(*_surface);

		fbo_update = true;
	}
	if (ui::SliderFloat("preview scale", &_previewScale, 0.0f, 5.0f)) {
		fbo_update = true;
	}
	if (ui::SliderFloat("preview gamma", &_previewGamma, 0.0f, 4.0f)) {
		fbo_update = true;
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
