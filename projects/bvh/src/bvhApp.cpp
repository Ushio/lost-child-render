#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include "collision_triangle.hpp"
#include "bvh.hpp"
#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "random_engine.hpp"

#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
#include <boost/format.hpp>

using namespace ci;
using namespace ci::app;
using namespace std;

class bvhApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;
	cinder::TriMeshRef _mesh;
	lc::BVH _bvh;

	bool _show_all = false;
	int _show_depth = 0;
};

void bvhApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

	cinder::ObjLoader loader(loadAsset("bunny.obj"));
	//cinder::ObjLoader loader(loadAsset("lucy.obj"));
	_mesh = cinder::TriMesh::create(loader);
	_bvh.set_triangle(lc::to_triangles(_mesh));
	_bvh.build();
}

void bvhApp::mouseDown(MouseEvent event)
{
}

void bvhApp::update()
{

}


void bvhApp::draw()
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

	{
		gl::ScopedPolygonMode wire(GL_LINE);
		gl::draw(*_mesh);
	}


	lc::DefaultEngine e;

	for (int i = 0; i < 50; ++i) {
		lc::Vec3 o = e.on_sphere() * glm::mix(1.5, 4.0, e.continuous());
		lc::Ray ray(o, glm::normalize(e.on_sphere() * 0.5 - o));

		if (auto intersection = _bvh.intersect(ray)) {
			auto p = intersection->intersect_position(ray);
			auto n = intersection->intersect_normal(_bvh._triangles[intersection->triangle_index]);
			auto r = glm::reflect(ray.d, n);

			gl::ScopedColor c(1.0, 0.5, 0.0);
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

	if (_show_all) {
		gl::VertBatch vb(GL_LINES);
		for (int i = 0; i < _bvh._nodes.size() ; ++i) {
			const lc::BVH::Node &node = _bvh._nodes[i];
			if (lc::empty(node.aabb) == false) {
				lc::draw_wire_aabb(node.aabb, vb);
			}
		}
		vb.draw();
	}
	else {
		int	node_begin = 0;
		int node_count = 1;
		for (int i = 0; i < _show_depth; ++i) {
			node_begin += node_count;
			node_count = node_count << 1;
		}

		gl::VertBatch vb(GL_LINES);
		for (int i = 0; i < node_count; ++i) {
			const lc::BVH::Node &node = _bvh._nodes[node_begin + i];
			if (lc::empty(node.aabb) == false) {
				lc::draw_wire_aabb(node.aabb, vb);
			}
		}
		vb.draw();
	}

	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::Checkbox("show all", &_show_all);
	if (_show_all == false) {
		ui::SliderInt("show depth", &_show_depth, 0, _bvh.depth_count() - 1);
	}
}

CINDER_APP(bvhApp, RendererGl)
