#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "helper_cinder/draw_camera.hpp"

#include "random_engine.hpp"
#include "bvh.hpp"
#include "camera.hpp"

#include <stack>
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
#include <boost/format.hpp>

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

	int _image_width = 640;
	int _image_height = 480;
	float _fovy = glm::radians(45.0);
	vec3 _eye = { 0.0, 0.0, 1.0 };

	cinder::TriMeshRef _mesh;
	lc::BVH _bvh;
};

void CameraApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

	cinder::ObjLoader loader(loadAsset("teapot.obj"));
	_mesh = cinder::TriMesh::create(loader);
	_bvh.set_triangle(lc::to_triangles(_mesh));
	_bvh.build();
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
	gl::drawCoordinateFrame();

	gl::drawSphere(_eye, 0.1f);

	double elapsed = getElapsedSeconds();
	lc::Mat4 model_matrix;
	model_matrix = glm::scale(model_matrix, lc::Vec3(0.5));
	model_matrix = glm::rotate(model_matrix, elapsed * 0.1, lc::Vec3(0.0, 1.0, 0.0));
	lc::Transform modelTransform(model_matrix);
	{
		gl::ScopedMatrices smat;
		gl::multModelMatrix(modelTransform.matrix());
		gl::ScopedGlslProg shader(gl::getStockShader(gl::ShaderDef().color().lambert()));

		gl::draw(*_mesh);
	}


	lc::Vec3 eye = _eye;
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };
	lc::Transform viewTransform(glm::lookAt(eye, look_at, up));

	lc::Transform modelViewTransform = viewTransform * modelTransform;

	lc::Camera::Settings camera_settings;
	camera_settings.eye = _eye;
	camera_settings.fovy = _fovy;
	lc::Camera camera(camera_settings);

	{
		gl::ScopedMatrices smat;
		gl::multModelMatrix(viewTransform.inverse_matrix());
		lc::draw_camera(camera, _image_width, _image_height, 5.0);
	}

	gl::ScopedColor color(1.0f, 0.8f, 0.0f);
	for (int h = 0; h < _image_height; h += 50) {
		for (int w = 0; w < _image_width; w += 50) {
			lc::Ray ray = camera.generate_ray(w, h, _image_width, _image_height);
			lc::Ray world_ray = viewTransform.to_local_ray(ray);
			lc::Ray local_ray = modelViewTransform.to_local_ray(ray);

			

			if (auto intersection = _bvh.intersect(local_ray)) {
				auto p = modelTransform.from_local_position(intersection->intersect_position(local_ray));
				auto n = modelTransform.from_local_normal(intersection->intersect_normal(_bvh._triangles[intersection->triangle_index]));

				auto r = glm::reflect(ray.d, n);

				gl::ScopedColor c(1.0, 0.0, 0.0);
				gl::drawLine(world_ray.o, p);
				gl::drawSphere(p, 0.01f);
				gl::drawLine(p, p + r * 0.1);
			} else {
				gl::drawLine(world_ray.o, world_ray.o + world_ray.d * 5.0);
			}
		}
	}

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::SliderInt("width", &_image_width, 0, 1920);
	ui::SliderInt("height", &_image_height, 0, 1080);
	ui::SliderFloat("fovy", &_fovy, 0.0, glm::radians(180.0));
	ui::SliderFloat3("eye", glm::value_ptr(_eye), -3.0f, 3.0f);
}

CINDER_APP(CameraApp, RendererGl)
