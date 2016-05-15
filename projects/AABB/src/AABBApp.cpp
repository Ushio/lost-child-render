#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "collision_aabb.hpp"
#include "random_engine.hpp"


using namespace ci;
using namespace ci::app;
using namespace std;

class AABBApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

};

void AABBApp::setup()
{
	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

}

void AABBApp::mouseDown(MouseEvent event)
{
}

void AABBApp::update()
{

}

void AABBApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}
	gl::drawCoordinateFrame();


	lc::AABB aabb(lc::Vec3(-1.0, -2.0, -1.0), lc::Vec3(1.0, 2.0, 1.0));
	
	gl::drawStrokedCube(cinder::AxisAlignedBox((vec3)aabb.min_position, (vec3)aabb.max_position));

	lc::Xor e;
	for (int i = 0; i < 100; ++i) {
		lc::Vec3 o = lc::generate_on_sphere(e) * glm::mix(1.5, 4.0, lc::generate_continuous(e));
		lc::Ray ray(o, lc::generate_continuous(e) < 0.2 ? lc::generate_on_sphere(e) : -o);

		if (auto intersection = lc::intersect(ray, aabb)) {
			auto p = intersection->intersect_position(ray);

			gl::ScopedColor red(1.0f, 0.0f, 0.0f);
			gl::drawCube((vec3)ray.o, vec3(0.05f));
			gl::drawLine(o, p);
		}
		else {
			gl::drawCube((vec3)ray.o, vec3(0.05f));
			gl::drawLine(ray.o, ray.o + ray.d * 10.0);
		}
	}
}

CINDER_APP(AABBApp, RendererGl)
