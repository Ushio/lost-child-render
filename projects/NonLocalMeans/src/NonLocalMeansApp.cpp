#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;



class NonLocalMeansApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

	Surface32fRef _input;
	Surface32fRef _output;
	gl::Texture2dRef _input_texture;
	gl::Texture2dRef _output_texture;
};

inline cinder::Surface32fRef non_local_means(cinder::Surface32fRef image) {
	int width = image->getWidth();
	int height = image->getHeight();
	auto surface = cinder::Surface32f::create(width, height, false);
	float *src = image->getData();
	float *dst = surface->getData();

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			auto sample = [=](int x, int y) {
				float *p = src + (y * width + x) * 3;
				return vec3(p[0], p[1], p[2]);
			};

			float *dst_pixel = src + (y * width + x) * 3;
			auto c = sample(x, y);
			for (int i = 0; i < 3; ++i) {
				dst_pixel[i] = c[i];
			}
		}
	}

	return surface;
}

void NonLocalMeansApp::setup()
{
	_input = Surface32f::create(loadImage(loadAsset("noisy.png")));
	_input_texture = gl::Texture2d::create(*_input);

	_output = non_local_means(_input);
	_output_texture = gl::Texture2d::create(*_input);
}

void NonLocalMeansApp::mouseDown( MouseEvent event )
{
}

void NonLocalMeansApp::update()
{
}

void NonLocalMeansApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
	gl::draw(_output_texture);
}

CINDER_APP( NonLocalMeansApp, RendererGl )
