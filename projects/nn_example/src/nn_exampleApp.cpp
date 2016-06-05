#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "CinderImGui.h"

#include "random_engine.hpp"
#include <boost/format.hpp>

// #include <nanoflann.hpp>
#include <kdtree.h>

using namespace ci;
using namespace ci::app;
using namespace std;

namespace lc {
	struct IrradianceCache {
		struct Irradiance {
			Vec3 e;
			Vec3 p;
			Vec3 n;
		};
		static void deleter(void *ptr) {
			delete (Irradiance *)ptr;
		}
		IrradianceCache() {
			_kd = kd_create(3);
			kd_data_destructor(_kd, deleter);
		}

		void add(const Vec3 &e, const Vec3 &p, const Vec3 &n) {
			Irradiance *irr = new Irradiance();
			irr->e = e;
			irr->p = p;
			irr->n = n;
			kd_insert3(_kd, p.x, p.y, p.z, irr);
			_irradiances.push_back(irr);
		}

		boost::optional<Vec3> irradiance(const const Vec3 &p, const Vec3 &n, double radius) const {
			kdres *presults = kd_nearest_range3(_kd, p.x, p.y, p.z, radius);

			Vec3 e;
			double weight_all = 0.0;
			while (!kd_res_end(presults)) {
				/* get the data and position of the current result item */
				lc::Vec3 cache_p;
				lc::IrradianceCache::Irradiance *irr = (lc::IrradianceCache::Irradiance *)kd_res_item(presults, glm::value_ptr(cache_p));

				double NoIN = glm::dot(n, irr->n);
				if (NoIN < 0.0001) {
					kd_res_next(presults);
					continue;
				}
				double eps = glm::distance(p, cache_p) / radius + glm::sqrt(1.0 - NoIN);
				double w = 1.0 / glm::max(eps, 0.00001);
				e += irr->e * w;
				weight_all *= w;
				kd_res_next(presults);
			}
			kd_res_free(presults);
			presults = nullptr;

			if (weight_all <= 0.00001) {
				return boost::none;
			}

			e /= weight_all;
			return e;
		}
		
		std::vector<Irradiance *> _irradiances;
		kdtree *_kd = nullptr;
	};
}



class nn_exampleApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	lc::IrradianceCache _cache;

	glm::vec3 _query;
	float _radius = 2.0f;
};

void nn_exampleApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 40.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(50.0f)).subdivisions(ivec2(10)), colorShader);

	
}

void nn_exampleApp::mouseDown(MouseEvent event)
{
}

void nn_exampleApp::update()
{
}

void nn_exampleApp::draw()
{
	gl::clear(Color(0, 0, 0));

	// Set up the camera.
	gl::ScopedMatrices push;
	gl::setMatrices(_camera);

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}
	static lc::MersenneTwister e;
	_cache.add(
		lc::Vec3(1.0),
		lc::Vec3(
			lc::generate_continuous(e, -10.0, 10.0),
			lc::generate_continuous(e, -10.0, 10.0),
			lc::generate_continuous(e, -10.0, 10.0)
		),
		lc::Vec3(0.0, 1.0, 0.0)
	);

	gl::VertBatch vb(GL_POINTS);

	for (lc::IrradianceCache::Irradiance *irr : _cache._irradiances) {
		vb.vertex(irr->p);
	}

	vb.draw();

	gl::drawSphere(_query, 0.2);

	{
		cinder::gl::ScopedPolygonMode wire(GL_LINE);
		cinder::gl::ScopedColor c(0.5, 0.5, 0.5);
		cinder::gl::drawSphere(_query, _radius, 15);
	}

	kdres *presults = kd_nearest_range3(_cache._kd, _query.x, _query.y, _query.z, _radius);
	while (!kd_res_end(presults)) {
		/* get the data and position of the current result item */
		lc::Vec3 p;
		lc::IrradianceCache::Irradiance *irr = (lc::IrradianceCache::Irradiance *)kd_res_item(presults, glm::value_ptr(p));
		
		cinder::gl::ScopedColor c(1.0, 0.0, 0.0);
		cinder::gl::drawSphere(p, 0.3f, 15);

		kd_res_next(presults);
	}
	kd_res_free(presults);
	presults = nullptr;

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::SliderFloat("x", &_query.x, -10.0f, 10.0f);
	ui::SliderFloat("y", &_query.y, -10.0f, 10.0f);
	ui::SliderFloat("z", &_query.z, -10.0f, 10.0f);
	ui::SliderFloat("radius", &_radius, 0.0f, 10.0f);
}

CINDER_APP(nn_exampleApp, RendererGl)
