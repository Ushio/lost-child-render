#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "random_engine.hpp"
#include "importance.hpp"
#include "brdf.hpp"
#include <boost/format.hpp>

#include <fpieee.h>
#include <excpt.h>
#include <float.h>
#include <stddef.h>

using namespace ci;
using namespace ci::app;
using namespace std;

int fpieee_handler(_FPIEEE_RECORD *);

int fpieee_handler(_FPIEEE_RECORD *pieee)
{
	// user-defined ieee trap handler routine:
	// there is one handler for all 
	// IEEE exceptions

	// Assume the user wants all invalid 
	// operations to return 0.

	if ((pieee->Cause.InvalidOperation) &&
		(pieee->Result.Format == _FpFormatFp32))
	{
		pieee->Result.Value.Fp32Value = 0.0F;

		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else
		return EXCEPTION_EXECUTE_HANDLER;
}

#define _EXC_MASK    \
    _EM_UNDERFLOW  + \
    _EM_OVERFLOW   + \
    _EM_ZERODIVIDE + \
    _EM_INEXACT

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
	//std::vector<int> v(5);
	//try {
	//	v.at(5);
	//}catch(std::exception &e) {
	//	console() << e.what() << std::endl;
	//}

	_controlfp_s(NULL, _EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INEXACT, _MCW_EM);


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

	lc::RandomEngine<lc::MersenneTwister> e;

	gl::VertBatch vb(GL_POINTS);

	int N = 50000;
	// int N = 5000;

	lc::Vec3 omega_o = glm::normalize(lc::Vec3(0.0, 1.0, 2.0));
	{
		gl::ScopedColor vc(1.0f, 0.5f, 0.5f);
		gl::drawVector(vec3(), omega_o);
	}
	

	auto p = (vec2)getMousePos() / (vec2)getDisplay()->getSize();
	double roughness = p.x;

	/*
	lc::Vec3 eye = _camera.getEyePoint();
	lc::Vec3 omega_o = glm::normalize(eye);

	for (int i = 0; i < N; ++i) {
		// auto eps = std::make_tuple(e.continuous(), e.continuous());
		auto n = e.on_hemisphere();
		lc::Gp(omega_o, )
	}
	*/

	double sum = 0.0;
	for (int i = 0; i < N; ++i) {
		lc::Vec3 n = glm::normalize(lc::Vec3(0.0, 1.0, 0.0));

		auto eps = std::make_tuple(e.continuous(), e.continuous());
		// auto sample = lc::importance_lambert(eps, n);
		auto sample = lc::importance_ggx(eps, n, omega_o, roughness);

		lc::Vec3 omega_i = sample.value.omega_i;
		double pdf = sample.pdf;


		if (glm::dot(omega_i, n) < 0.0) {
			continue;
		}

		// せっかくの重みが台無しではあるが、テストとして
		// 表面積（厚さ1の体積）
		sum += 1.0 / pdf;

		// PDFの分布可視化
		float c = pdf * 3.0f;
		vb.color(c, c, c);
		vb.vertex(omega_i);
	}

	if (getElapsedFrames() % 60 == 1) {
		double integral = sum / N;
		console() << boost::format("integral = %d, r = %.2f") % integral % roughness << std::endl;
	}


	vb.draw();
}

CINDER_APP( CosineScatterApp, RendererGl )
