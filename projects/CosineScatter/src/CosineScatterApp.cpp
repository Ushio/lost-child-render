#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "random_engine.hpp"
#include <boost/format.hpp>

using namespace ci;
using namespace ci::app;
using namespace std;

class CosineScatterApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;
};

void CosineScatterApp::setup()
{
	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);
}

void CosineScatterApp::mouseDown( MouseEvent event )
{
}

void CosineScatterApp::update()
{
}

void CosineScatterApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}


	lc::MersenneTwister e;

	gl::VertBatch vb(GL_POINTS);

	int N = 10000;

	double sum = 0.0;
	for (int i = 0; i < N; ++i) {
		auto s = lc::generate_cosine_weight_hemisphere(e);
		vb.vertex(s.value);

		// せっかくの重みが台無しではあるが、テストとして
		// 表面積（厚さ1の体積）
		sum += 1.0 / s.pdf;
	}
	if (getElapsedFrames() % 60 == 0) {
		double integral = sum / N;
		console() << boost::format("integral = %d") % integral << std::endl;
	}

	vb.draw();
}

CINDER_APP( CosineScatterApp, RendererGl )
