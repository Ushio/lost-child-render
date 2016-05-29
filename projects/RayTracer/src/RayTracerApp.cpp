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
	typedef Xor EngineType;

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
			trace_p = 0.0;
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

		trace_p = glm::max(trace_p, 0.00001);

		if (trace_p <= generate_continuous(engine)) {
			// ロシアンルーレットによる終了
			return Vec3();
		}

		if (auto lambert = boost::get<LambertMaterial>(&intersection->m)) {
			Vec3 omega_o = -ray.d;

			auto sample = generate_cosine_weight_hemisphere(engine);

			// 半球定義
			Vec3 yaxis = intersection->n;
			Vec3 xaxis;
			Vec3 zaxis;
			if (0.999f < fabs(yaxis.z)) {
				xaxis = normalize(cross(Vec3(0.0, -1.0, 0.0), yaxis));
			}
			else {
				xaxis = normalize(cross(Vec3(0.0, 0.0, 1.0), yaxis));
			}
			zaxis = cross(xaxis, yaxis);

			Vec3 omega_i = sample.value.x * xaxis + sample.value.y * yaxis + sample.value.z * zaxis;

			Ray new_ray;
			new_ray.o = intersection->p + omega_i * kReflectionBias;
			new_ray.d = omega_i;

			double cos_term = glm::dot(intersection->n, omega_i);
			double brdf = glm::one_over_pi<double>();
			Vec3 L = radiance(new_ray, scene, engine, lambert->albedo, depth + 1) / trace_p;
			return lambert->albedo * brdf * cos_term * L / sample.pdf;
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
};

void RayTracerApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 6.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

	//cinder::ObjLoader loader(loadAsset("bunny.obj"));
	//_mesh = cinder::TriMesh::create(loader);
	//_bvh.set_triangle(lc::to_triangles(_mesh));
	//_bvh.build();

	_buffer = new lc::AccumlationBuffer(wide, wide);

	lc::Vec3 eye(0.0, 0.0, 8.0);
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };

	lc::Camera::Settings camera_settings;
	camera_settings.fovy = glm::radians(60.0);
	_scene.camera = lc::Camera(camera_settings);
	_scene.viewTransform = lc::Transform(glm::lookAt(eye, look_at, up));

	_scene.objects.push_back(lc::ConelBoxObject(5.0));
	_scene.objects.push_back(lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0, -1.5, 0.0), 1.0),
		lc::LambertMaterial(lc::Vec3(1.0))
	));

	auto light = lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0, 2.5, 0.0), 1.0),
		lc::EmissiveMaterial(lc::Vec3(5.0))
	);
	_scene.objects.push_back(light);

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

	lc::draw_scene(_scene, wide, wide);
	
	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	if (ui::Button("step")) {
		for (int i = 0; i < 10; ++i) {
			lc::step(*_buffer, _scene);
		}
		_surface = lc::to_surface(*_buffer);
		_texture = gl::Texture2d::create(*_surface);
	}

	if (_texture) {
		ui::Image(_texture, ImVec2(_texture->getWidth(), _texture->getHeight()));
	}
}

CINDER_APP(RayTracerApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(false);
});
