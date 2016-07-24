#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "CinderImGui.h"

#include <ppl.h>

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

	gl::GlslProgRef _previewShader;

	float _sigma = 0.4f;
	float _param_h = 0.4f;
};

inline cinder::Surface32fRef non_local_means(cinder::Surface32fRef image, float param_h, float sigma) {
	param_h = std::max(0.0001f, param_h);
	sigma = std::max(0.0001f, sigma);

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
	concurrency::parallel_for<int>(0, height, [=](int y) {
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
					auto arg = -glm::max(dist - 2.0f * sigma * sigma, 0.0f) / (param_h * param_h);
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
	});

	return surface;
}
cinder::Surface8uRef gamma_correction(cinder::Surface32fRef surface) {
	cinder::Surface32f cp = surface->clone();
	int w = surface->getWidth();
	int h = surface->getHeight();

	// ÉKÉìÉ}ï‚ê≥
	float gamma_coef = 1.0f / 2.2f;
	concurrency::parallel_for<int>(0, h, [=, &cp](int y) {
		float *lineHead = cp.getData(glm::ivec2(0, y));
		for (int x = 0; x < w; ++x) {
			float *rgb = lineHead + x * 3;
			for (int i = 0; i < 3; ++i) {
				rgb[i] = pow(rgb[i], gamma_coef);
			}
		}
	});
	return cinder::Surface8u::create(cp);
}
void NonLocalMeansApp::setup()
{
	ui::initialize();

	_input = Surface32f::create(loadImage(loadAsset("image.exr")));
	_input_texture = gl::Texture2d::create(*_input);

	_output = non_local_means(_input, _param_h, _sigma);
	_output_texture = gl::Texture2d::create(*_output);

	_previewShader = gl::GlslProg::create(gl::GlslProg::Format()
		.vertex(loadAsset("preview_shader/shader.vert"))
		.fragment(loadAsset("preview_shader/shader.frag")));
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

	static bool showOriginal = false;

	gl::Texture2dRef texture = showOriginal ? _input_texture : _output_texture;


	gl::ScopedGlslProg shaderScp(_previewShader);
	gl::ScopedTextureBind texBindScp(texture);
	_previewShader->uniform("u_scale", 1.0f);
	_previewShader->uniform("u_gamma_correction", 1.0f / 2.2f);
	gl::drawSolidRect(texture->getBounds());

	ui::ScopedWindow window("params", glm::vec2(200, 300));

	ui::Checkbox("show original", &showOriginal);

	ui::InputFloat("h", &_param_h);
	ui::InputFloat("sigma", &_sigma);

	if (_output && ui::Button("update")) {
		_output = non_local_means(_input, _param_h, _sigma);
		_output_texture = gl::Texture2d::create(*_output);
	}
	if (_output && ui::Button("save")) {
		auto dstPath = getAssetPath("") / "output_image.png";
		writeImage(dstPath, *gamma_correction(_output), ImageTarget::Options().quality(1.0f), "png");
	}
}

CINDER_APP( NonLocalMeansApp, RendererGl )
