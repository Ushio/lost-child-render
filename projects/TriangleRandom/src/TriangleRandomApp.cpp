#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "random_engine.hpp"
#include "triangle_area.hpp"
#include "uniform_on_triangle.hpp"

#include <boost/format.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

using namespace ci;
using namespace ci::app;
using namespace std;

class TriangleRandomApp : public App {
public:
	void setup() override;
	void keyDown(KeyEvent event) {
		_index++;
	}
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	int _index = 0;

	lc::UniformOnTriangle _uniformTri;
};

void TriangleRandomApp::setup()
{
	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);


	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	std::string path = (getAssetPath("") / "butterfly.obj").string();
	bool ret = tinyobj::LoadObj(shapes, materials, err, path.c_str());

	const tinyobj::shape_t &shape = shapes[0];
	std::vector<lc::Triangle> triangles;
	for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
		lc::Triangle tri;
		for (int j = 0; j < 3; ++j) {
			int idx = shape.mesh.indices[i + j];
			for (int k = 0; k < 3; ++k) {
				tri.v[j][k] = shape.mesh.positions[idx * 3 + k];
			}
		}
		triangles.push_back(tri);
	}

	_uniformTri.set_triangle(triangles);
	_uniformTri.build();

	// 面積検証コード
	/*
	lc::RandomEngine<lc::Xor> e;
	e.discard(100);
	for (int i = 0; i < 100; ++i) {
		lc::Vec3 p0(0.0, 0.0, 0.0);
		lc::Vec3 p1(10.0, 0.0, 0.0);
		lc::Vec3 p2(10.0, 10.0, 0.0);
		double base_area = 50.0;

		float scale = e.continuous(0.1f, 10.0f);

		// 回転、移動が行われても、正しく面積を求めることができるはずである
		lc::Vec3 translation(e.continuous(), e.continuous(), e.continuous());
		lc::Quat rotation = glm::angleAxis(e.continuous(0.0, glm::pi<double>()), e.on_sphere());
		p0 = rotation * (p0 + translation);
		p1 = rotation * (p1 + translation);
		p2 = rotation * (p2 + translation);

		p0 *= scale;
		p1 *= scale;
		p2 *= scale;

		double area = lc::triangle_area(p0, p1, p2);

		// スケールがx倍されるとき、面積はx^2倍になるはずである
		double d = area - base_area * scale * scale;
		if (0.000001 < d) {
			abort();
		}
	}
	*/
}

void TriangleRandomApp::mouseDown(MouseEvent event)
{
}

void TriangleRandomApp::update()
{
}

void TriangleRandomApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}

	gl::VertBatch vb(GL_POINTS);

	static lc::RandomEngine<lc::Xor128> e_triin;
	for (int i = 0; i < 10000; ++i) {
		lc::Vec3 p = _uniformTri.uniform(e_triin);
		vb.vertex(p);
	}

	vb.draw();

	for (int i = 0; i < _uniformTri._triangles.size(); ++i) {
		auto tri = _uniformTri._triangles[i];
		for (int j = 0; j < 3; ++j) {
			gl::drawLine(tri[j], tri[(j + 1) % 3]);
		}
	}
}

CINDER_APP(TriangleRandomApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(true);
})