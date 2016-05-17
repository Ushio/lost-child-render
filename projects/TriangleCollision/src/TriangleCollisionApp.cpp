#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"

#include "collision_triangle.hpp"
#include "random_engine.hpp"
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>

using namespace ci;
using namespace ci::app;
using namespace std;

class TriangleCollisionApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

};

void TriangleCollisionApp::setup()
{
	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

}

void TriangleCollisionApp::mouseDown(MouseEvent event)
{
}

void TriangleCollisionApp::update()
{

}

void TriangleCollisionApp::draw()
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


	lc::Xor e;

	struct ColorTriangle {
		lc::Triangle triangle;
		lc::Vec3 color;
	};
	std::vector<ColorTriangle> triangles;

	ColorTriangle tri;
	tri.triangle = lc::Triangle(
		lc::Vec3(-1.0, -1.0, 0.0),
		lc::Vec3(1.0, -1.0, 0.0),
		lc::Vec3(0.0, 1.5, 0.0)
	);
	tri.color = lc::Vec3(1.0, 1.0, 0.0);
	triangles.emplace_back(tri);

	double tri_range = 1.0;
	double tri_size = 0.8;
	for (int i = 0; i < 5; ++i) {
		ColorTriangle tri;
		lc::Vec3 center(lc::generate_continuous(e, -tri_range, tri_range), lc::generate_continuous(e, -tri_range, tri_range), lc::generate_continuous(e, -tri_range, tri_range));
		tri.triangle = lc::Triangle(
			center + lc::Vec3(lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size)),
			center + lc::Vec3(lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size)),
			center + lc::Vec3(lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size))
		);
		tri.color = glm::rgbColor(lc::Vec3(lc::generate_continuous(e) * 360.0, 1.0, 1.0));
		triangles.emplace_back(tri);
	}

	gl::VertBatch tri_vb(GL_LINES);

	for (int i = 0; i < triangles.size(); ++i) {
		auto triangle = triangles[i];
		tri_vb.color(triangle.color.r, triangle.color.g, triangle.color.b);

		// 三角形
		for (int j = 0; j < 3; ++j) {
			tri_vb.vertex(triangle.triangle.v[j]);
			tri_vb.vertex(triangle.triangle.v[(j + 1) % 3]);
		}

		// 法線
		lc::Vec3 c = boost::accumulate(triangle.triangle.v, lc::Vec3()) / triangle.triangle.v.size();
		lc::Vec3 n = lc::triangle_normal(triangle.triangle, false);
		gl::ScopedColor n_color(triangle.color.r, triangle.color.g, triangle.color.b);
		gl::drawVector(c, c + n * 0.4, 0.1f, 0.02f);
	}
	tri_vb.draw();

	for (int i = 0; i < 50; ++i) {
		lc::Vec3 o = lc::generate_on_sphere(e) * glm::mix(1.5, 4.0, lc::generate_continuous(e));
		lc::Ray ray(o, glm::normalize(lc::generate_on_sphere(e) * 0.5 - o));

		int intersection_index;
		boost::optional<lc::TriangleIntersection> intersection;
		for (int j = 0; j < triangles.size(); ++j) {
			if (auto new_intersection = lc::intersect(ray, triangles[j].triangle)) {
				double tmin = intersection ? intersection->tmin : std::numeric_limits<double>::max();
				if (new_intersection->tmin <= tmin) {
					intersection = new_intersection;
					intersection_index = j;
				}
			}
		}

		if (intersection) {
			auto tri_color = triangles[intersection_index].color;
			auto p = intersection->intersect_position(ray);
			auto n = intersection->intersect_normal(triangles[intersection_index].triangle);
			auto r = glm::reflect(ray.d, n);

			gl::ScopedColor c(tri_color.r, tri_color.g, tri_color.b);
			gl::drawCube((vec3)ray.o, vec3(0.05f));
			gl::drawLine(o, p);
			gl::drawSphere(p, 0.01f);
			gl::drawLine(p, p + r * 0.1);
		}
		else {
			gl::ScopedColor c(0.5f, 0.5f, 0.5f);
			gl::drawCube((vec3)ray.o, vec3(0.05f));
			gl::drawLine(ray.o, ray.o + ray.d * 5.0);
		}
	}
}

CINDER_APP(TriangleCollisionApp, RendererGl)
