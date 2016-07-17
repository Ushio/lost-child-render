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
	const int kKernel = 5;
	const int kSupport = 13;
	const int kHalfKernel = kKernel / 2;
	const int kHalfSupport = kSupport / 2;

	int width = image->getWidth();
	int height = image->getHeight();
	auto surface = cinder::Surface32f::create(width, height, false);
	float *src = image->getData();
	float *dst = surface->getData();

	typedef std::array<float, 3 * kKernel * kKernel> Template;

	auto sample_template = [width, height, src, kHalfKernel](int x, int y) {
		Template t;
		int index = 0;
		for (int sx = x - kHalfKernel; sx <= x + kHalfKernel; ++sx) {
			for (int sy = y - kHalfKernel; sy <= y + kHalfKernel; ++sy) {
				int sample_x = sx;
				int sample_y = sy;
				sample_x = std::max(sample_x, 0);
				sample_x = std::min(sample_x, width - 1);
				sample_y = std::max(sample_y, 0);
				sample_y = std::min(sample_y, height - 1);

				float *p = src + (sample_y * width + sample_x) * 3;
				for (int i = 0; i < 3; ++i) {
					t[index++] = p[i];
				}
			}
		}
		return t;
	};
	auto distance_sqared_template = [](const Template &a, const Template &b) {
		float accum = 0.0f;
		for (int i = 0; i < a.size(); ++i) {
			accum += glm::pow(a[i] - b[i], 2);
		}
		return accum;
	};

	float h = 0.4f;
	float sigma = 0.4f;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			auto sample = [=](int x, int y) {
				int sample_x = x;
				int sample_y = y;
				sample_x = std::max(sample_x, 0);
				sample_x = std::min(sample_x, width - 1);
				sample_y = std::max(sample_y, 0);
				sample_y = std::min(sample_y, height - 1);

				float *p = src + (sample_y * width + sample_x) * 3;
				return vec3(p[0], p[1], p[2]);
			};

			float *dst_pixel = dst + (y * width + x) * 3;

			auto focus = sample_template(x, y);

			dvec3 sum;
			double sum_weight = 0.0;
			for (int sx = x - kHalfSupport; sx <= x + kHalfSupport; ++sx) {
				for (int sy = y - kHalfSupport; sy <= y + kHalfSupport; ++sy) {
					auto target = sample_template(sx, sy);
					auto dist = distance_sqared_template(focus, target);
					auto arg = -glm::max(dist - 2.0f * sigma * sigma, 0.0f) / (h * h);
					auto weight = glm::exp(arg);

					sum_weight += weight;
					sum += sample(sx, sy) * weight;
				}
			}
			auto color = sum / sum_weight;

			for (int i = 0; i < 3; ++i) {
				dst_pixel[i] = color[i];
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
	_output_texture = gl::Texture2d::create(*_output);
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
	// gl::draw(_input_texture);
	//gl::draw(_output_texture, 
	//	cinder::Area(_output_texture->getSize() / 2, _output_texture->getSize()),
	//	cinder::Rectf(_output_texture->getSize() / 2, _output_texture->getSize())
	//);
	gl::draw(_output_texture);
}

CINDER_APP( NonLocalMeansApp, RendererGl )
