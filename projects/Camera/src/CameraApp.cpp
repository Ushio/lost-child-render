#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include "collision_triangle.hpp"
#include "bvh.hpp"
#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "random_engine.hpp"
#include "transform.hpp"

#include <stack>
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
#include <boost/format.hpp>


namespace lc {
	// äÓñ{ÇÕçÇÇ≥ÇäÓèÄÇ…
	class Camera {
	public:
		double fovy = glm::radians(45.0);

		/*
		image_x: 0 to (image_width - 1)
		image_y: 0 to (image_height - 1)
		*/
		Ray generate_ray(double image_x, double image_y, int image_width, int image_height) const {
			double aspect = double(image_height) / double(image_width);
			double top = glm::tan(fovy * 0.5);
			double bottom = -top;
			double right = top * aspect;
			double left = -right;

			Vec3 to(
				left + (right - left) * image_x / double(image_width - 1),
				bottom + (top - bottom) * image_y / double(image_height - 1),
				-1.0
			);

			return Ray(Vec3(), glm::normalize(to));
		}
	};

	void draw_camera(const Camera &camera, int image_width, int image_height, double ray_length) {
		Ray ray0 = camera.generate_ray(0.0, 0.0, image_width, image_height);
		Ray ray1 = camera.generate_ray(image_width - 1.0, 0.0, image_width, image_height);
		Ray ray2 = camera.generate_ray(image_width - 1.0, image_height - 1.0, image_width, image_height);
		Ray ray3 = camera.generate_ray(0.0, image_height - 1.0, image_width, image_height);

		cinder::gl::drawLine(ray0.o, ray0.o + ray0.d * ray_length);
		cinder::gl::drawLine(ray1.o, ray1.o + ray1.d * ray_length);
		cinder::gl::drawLine(ray2.o, ray2.o + ray2.d * ray_length);
		cinder::gl::drawLine(ray3.o, ray3.o + ray3.d * ray_length);

		int N = 3;
		for (int i = 0; i < N; ++i) {
			Vec3 p0 = ray0.o + ray0.d * ray_length * double(i + 1) / double(N);
			Vec3 p1 = ray1.o + ray1.d * ray_length * double(i + 1) / double(N);
			Vec3 p2 = ray2.o + ray2.d * ray_length * double(i + 1) / double(N);
			Vec3 p3 = ray3.o + ray3.d * ray_length * double(i + 1) / double(N);

			cinder::gl::drawLine(p0, p1);
			cinder::gl::drawLine(p1, p2);
			cinder::gl::drawLine(p2, p3);
			cinder::gl::drawLine(p3, p0);
		}
	}
}

using namespace ci;
using namespace ci::app;
using namespace std;

class CameraApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	int _image_width = 320;
	int _image_height = 480;
};

void CameraApp::setup()
{
	ui::initialize();


	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);
}

void CameraApp::mouseDown(MouseEvent event)
{
}

void CameraApp::update()
{

}


void CameraApp::draw()
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

	double elapsed = getElapsedSeconds();
	lc::Mat4 mat;

	lc::Camera camera;
	lc::draw_camera(camera, _image_width, _image_height, 5.0);

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
}

CINDER_APP(CameraApp, RendererGl)
