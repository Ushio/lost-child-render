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

void draw_wire_aabb(const lc::AABB &aabb, gl::VertBatch &vb) {
	lc::Vec3 x_axis(aabb.max_position.x - aabb.min_position.x, 0.0, 0.0);
	lc::Vec3 z_axis(0.0, 0.0, aabb.max_position.z - aabb.min_position.z);

	std::array<lc::Vec3, 4> top = {
		aabb.max_position - x_axis - z_axis,
		aabb.max_position - x_axis,
		aabb.max_position,
		aabb.max_position - z_axis
	};
	std::array<lc::Vec3, 4> bottom = {
		aabb.min_position,
		aabb.min_position + z_axis,
		aabb.min_position + x_axis + z_axis,
		aabb.min_position + x_axis
	};
	for (int i = 0; i < 4; ++i) {
		vb.vertex(top[i]);
		vb.vertex(top[(i + 1) % 4]);
	}

	for (int i = 0; i < 4; ++i) {
		vb.vertex(bottom[i]);
		vb.vertex(bottom[(i + 1) % 4]);
	}
	for (int i = 0; i < 4; ++i) {
		vb.vertex(top[i]);
		vb.vertex(bottom[i]);
	}
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
	
	gl::VertBatch tri_vb(GL_LINES);
	draw_wire_aabb(aabb, tri_vb);
	tri_vb.draw();
	// gl::drawStrokedCube(cinder::AxisAlignedBox((vec3)aabb.min_position, (vec3)aabb.max_position));

	lc::DefaultEngine e;
	for (int i = 0; i < 100; ++i) {
		lc::Vec3 o = e.on_sphere() * glm::mix(1.5, 4.0, e.continuous());
		lc::Ray ray(o, e.continuous() < 0.2 ? e.on_sphere() : -o);

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
