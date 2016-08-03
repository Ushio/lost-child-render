#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "random_engine.hpp"
#include "triangle_area.hpp"
#include <boost/format.hpp>

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
};

void TriangleRandomApp::setup()
{
	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);


	// 面積確認コード
	lc::RandomEngine<lc::MersenneTwister> e;
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

	lc::RandomEngine<lc::MersenneTwister> e;
	e.discard(51 + _index);

	double triscale = 1.0;
	lc::Vec3 tri[] = {
		lc::Vec3(e.continuous(-triscale, triscale), e.continuous(-triscale, triscale), e.continuous(-triscale, triscale)),
		lc::Vec3(e.continuous(-triscale, triscale), e.continuous(-triscale, triscale), e.continuous(-triscale, triscale)),
		lc::Vec3(e.continuous(-triscale, triscale), e.continuous(-triscale, triscale), e.continuous(-triscale, triscale)),
	};

	for (int i = 0; i < 3; ++i) {
		gl::drawLine(tri[i], tri[(i + 1) % 3]);
	}

	gl::VertBatch vb(GL_POINTS);

	static lc::RandomEngine<lc::MersenneTwister> e_triin;
	for (int i = 0; i < 10000; ++i) {
		double r1 = e_triin.continuous();
		double r2 = e_triin.continuous();

		double sqrt_r1 = glm::sqrt(r1);
		lc::Vec3 p =
			(1.0 - sqrt_r1) * tri[0]
			+ sqrt_r1 * (1.0 - r2) * tri[1]
			+ sqrt_r1 * r2 * tri[2];
		vb.vertex(p);
	}

	vb.draw();
}

CINDER_APP(TriangleRandomApp, RendererGl)