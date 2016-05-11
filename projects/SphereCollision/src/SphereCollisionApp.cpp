#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "CinderImGui.h"

#include "collision_sphere.hpp"
#include "random_engine.hpp"
#include "refraction.hpp"


using namespace ci;
using namespace ci::app;
using namespace std;


class SphereCollisionApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	float _sphere_ior = 1.4f;
};

void SphereCollisionApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);
}

void SphereCollisionApp::mouseDown( MouseEvent event )
{
}

void SphereCollisionApp::update()
{

}

void trace_refract(const lc::Ray &ray, const lc::Sphere &sphere, double sphere_ior, int depth = 0) {
	if (5 <= depth) {
		return;
	}
	vec3 color(1.0);
	if (depth == 1) {
		color = vec3(1.0, 0.0, 0.0);
	}
	else if (depth == 2) {
		color = vec3(1.0, 1.0, 0.0);
	}
	gl::ScopedColor c(vec4(color, 1.0f));

	if (auto intersection = lc::intersect(ray, sphere)) {
		lc::Intersection i = *intersection;
		gl::drawLine(ray.o, intersection->p);

		double air_ior = 1.0;
		double eta = intersection->isback ? sphere_ior / air_ior : air_ior / sphere_ior;

		auto refract_dir = lc::refraction(ray.d, intersection->n, eta);
		trace_refract(lc::Ray(intersection->p + refract_dir * 0.001, refract_dir), sphere, sphere_ior, depth + 1);
	}
	else {
		gl::drawLine(ray.o, ray.o + ray.d * 100.0);
	}
}

void SphereCollisionApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}

	lc::Sphere sphere;
	
	{
		gl::ScopedPolygonMode wire(GL_LINE);
		gl::drawSphere(sphere.center, sphere.radius, 15);
	}

	
	lc::Xor engine;
	for (int i = 0; i < 100; ++i) {
		double x = (lc::generate_continuous(engine) - 0.5) * 1.7;
		double z = (lc::generate_continuous(engine) - 0.5) * 1.7;
		lc::Ray ray(lc::Vec3(x, 2.0, z), lc::Vec3(0.0, -1.0, 0.0));

		trace_refract(ray, sphere, _sphere_ior);
	}

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::SliderFloat("sphere ior", &_sphere_ior, 0.6f, 2.0f);
}

CINDER_APP( SphereCollisionApp, RendererGl )
