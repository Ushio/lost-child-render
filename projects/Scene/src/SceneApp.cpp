#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

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




using namespace ci;
using namespace ci::app;
using namespace std;

class SceneApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;
	
	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	//cinder::TriMeshRef _mesh;
	//lc::BVH _bvh;
};

void SceneApp::setup()
{
	ui::initialize();

	lc::Scene scene;

	_camera.lookAt(vec3(0, 0.0f, 6.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

	//cinder::ObjLoader loader(loadAsset("bunny.obj"));
	//_mesh = cinder::TriMesh::create(loader);
	//_bvh.set_triangle(lc::to_triangles(_mesh));
	//_bvh.build();
}

void SceneApp::mouseDown(MouseEvent event)
{
}

void SceneApp::update()
{

}


void SceneApp::draw()
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

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	
	lc::Scene scene;

	lc::Vec3 eye(0.0, 0.0, 8.0);
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };

	lc::Camera::Settings camera_settings;
	camera_settings.fovy = glm::radians(60.0);
	scene.camera = lc::Camera(camera_settings);
	scene.viewTransform = lc::Transform(glm::lookAt(eye, look_at, up));

	scene.objects.push_back(lc::ConelBoxObject(5.0));
	scene.objects.push_back(lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0), 1.0),
		lc::LambertMaterial(lc::Vec3(1.0))
	));

	scene.importances.push_back(lc::ImportantArea(lc::Sphere(lc::Vec3(0.0), 1.0)));
	// scene.importances.push_back(lc::ImportantArea(lc::Sphere(lc::Vec3(0.0, 2.0, 0.0), 1.0)));

	int wide = 512;
	lc::draw_scene(scene, wide, wide);

	// レイテスト
	lc::Xor xor;
	for (int i = 0; i < 100; ++i) {
		lc::Ray ray;

		if (i % 2 == 0) {
			ray = scene.camera.generate_ray(
				lc::generate_continuous(xor, 0.0, wide - 1),
				lc::generate_continuous(xor, 0.0, wide - 1),
				wide, wide);
			ray = scene.viewTransform.to_local_ray(ray);
		}
		else {
			auto s = lc::sample_important(scene, eye, xor);
			ray = lc::Ray(eye, glm::normalize(s.value - eye));

			// console() << "pdf: " << s.pdf << std::endl;
		}

		if (auto intersect = lc::intersect(ray, scene)) {
			gl::ScopedColor color(1.0f, 0.5f, 0.0f);
			gl::drawLine(ray.o, intersect->p);
			gl::drawSphere(intersect->p, 0.05f);

			auto r = glm::reflect(ray.d, intersect->n);
			gl::drawLine(intersect->p, intersect->p + r * 1.0);

			gl::ScopedColor color_2(0.8f, 0.8f, 0.0f);
			gl::drawLine(intersect->p, intersect->p + intersect->n * 0.5);
		}
		else {
			gl::ScopedColor color(0.0f, 0.0f, 0.5f);
			gl::drawLine(ray.o, ray.o + ray.d * 20.0);
		}
	}
}

CINDER_APP(SceneApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(false);
});
