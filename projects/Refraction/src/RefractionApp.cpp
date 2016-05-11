#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "CinderImGui.h"

#include "collision_plane.hpp"
#include "random_engine.hpp"
#include "refraction.hpp"


using namespace ci;
using namespace ci::app;
using namespace std;

class RefractionApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	float _floor_ior = 1.4f;
};

void RefractionApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);
}

void RefractionApp::mouseDown(MouseEvent event)
{
}

void RefractionApp::update()
{

}

void RefractionApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}

	lc::Plane plane = lc::make_plane_pn(lc::Vec3(), lc::Vec3(0.0, 1.0, 0.0));

	int N = 100;
	for (int i = 0; i < N; ++i) {
		double s = (double)i / (N - 1);
		double r = glm::mix(glm::pi<double>(), 0.0, s);
		lc::Ray ray(lc::Vec3(0.0, -1.0, 0.0), lc::Vec3(glm::cos(r), glm::sin(r), 0.0));

		if (auto intersection = lc::intersect(ray, plane)) {
			lc::Intersection i = *intersection;
			gl::drawLine(ray.o, intersection->p);

			double air_ior = 1.0;
			double eta = intersection->isback ? _floor_ior / air_ior : air_ior / _floor_ior;

			auto refract_dir = lc::refraction(ray.d, intersection->n, eta);

			gl::ScopedColor c(1.0f, 1.0f, 0.0f, 1.0f);
			gl::drawLine(intersection->p, intersection->p + refract_dir * 5.0);
		}
		else {
			gl::drawLine(ray.o, ray.o + ray.d * 5.0);
		}
	}

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::SliderFloat("floor ior", &_floor_ior, 0.6f, 2.0f);
}

CINDER_APP(RefractionApp, RendererGl)
