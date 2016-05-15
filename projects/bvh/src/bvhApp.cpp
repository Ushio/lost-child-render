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

	{
		gl::ScopedColor color(Color::gray(0.2f));
		_plane->draw();
	}
	gl::drawCoordinateFrame();

	{
		gl::ScopedPolygonMode wire(GL_LINE);
		gl::draw(*_mesh);
	}


	lc::Xor e;

	//struct ColorTriangle {
	//	lc::Triangle triangle;
	//	lc::Vec3 color;
	//};
	//std::vector<ColorTriangle> triangles;

	//ColorTriangle tri;
	//tri.triangle = lc::Triangle(
	//	lc::Vec3(-1.0, -1.0, 0.0),
	//	lc::Vec3(1.0, -1.0, 0.0),
	//	lc::Vec3(0.0, 1.5, 0.0)
	//);
	//tri.color = lc::Vec3(1.0, 1.0, 0.0);
	//triangles.emplace_back(tri);

	//double tri_range = 1.0;
	//double tri_size = 0.8;
	//for (int i = 0; i < 5; ++i) {
	//	ColorTriangle tri;
	//	lc::Vec3 center(lc::generate_continuous(e, -tri_range, tri_range), lc::generate_continuous(e, -tri_range, tri_range), lc::generate_continuous(e, -tri_range, tri_range));
	//	tri.triangle = lc::Triangle(
	//		center + lc::Vec3(lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size)),
	//		center + lc::Vec3(lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size)),
	//		center + lc::Vec3(lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size), lc::generate_continuous(e, -tri_size, tri_size))
	//	);
	//	tri.color = glm::rgbColor(lc::Vec3(lc::generate_continuous(e) * 360.0, 1.0, 1.0));
	//	triangles.emplace_back(tri);
	//}

	//gl::VertBatch tri_vb(GL_LINES);

	//for (int i = 0; i < triangles.size(); ++i) {
	//	auto triangle = triangles[i];
	//	tri_vb.color(triangle.color.r, triangle.color.g, triangle.color.b);

	//	// 三角形
	//	for (int j = 0; j < 3; ++j) {
	//		tri_vb.vertex(triangle.triangle.v[j]);
	//		tri_vb.vertex(triangle.triangle.v[(j + 1) % 3]);
	//	}

	//	// 法線
	//	lc::Vec3 c = boost::accumulate(triangle.triangle.v, lc::Vec3()) / triangle.triangle.v.size();
	//	lc::Vec3 n = lc::triangle_normal(triangle.triangle, false);
	//	gl::ScopedColor n_color(triangle.color.r, triangle.color.g, triangle.color.b);
	//	gl::drawVector(c, c + n * 0.4, 0.1f, 0.02f);
	//}
	//tri_vb.draw();

	for (int i = 0; i < 50; ++i) {
		lc::Vec3 o = lc::generate_on_sphere(e) * glm::mix(1.5, 4.0, lc::generate_continuous(e));
		lc::Ray ray(o, glm::normalize(lc::generate_on_sphere(e) * 0.5 - o));

		//int intersection_index;
		//boost::optional<lc::TriangleIntersection> intersection;
		//for (int j = 0; j < triangles.size(); ++j) {
		//	if (auto new_intersection = lc::intersect(ray, triangles[j].triangle)) {
		//		double tmin = intersection ? intersection->tmin : std::numeric_limits<double>::max();
		//		if (new_intersection->tmin <= tmin) {
		//			intersection = new_intersection;
		//			intersection_index = j;
		//		}
		//	}
		//}

		//if (intersection) {
		//	auto tri_color = triangles[intersection_index].color;
		//	auto p = intersection->intersect_position(ray);
		//	auto n = intersection->intersect_normal(triangles[intersection_index].triangle);
		//	auto r = glm::reflect(ray.d, n);

		//	gl::ScopedColor c(tri_color.r, tri_color.g, tri_color.b);
		//	gl::drawCube((vec3)ray.o, vec3(0.05f));
		//	gl::drawLine(o, p);
		//	gl::drawSphere(p, 0.01f);
		//	gl::drawLine(p, p + r * 0.1);
		//}
		//else {
		//	gl::ScopedColor c(0.5f, 0.5f, 0.5f);
		//	gl::drawCube((vec3)ray.o, vec3(0.05f));
		//	gl::drawLine(ray.o, ray.o + ray.d * 5.0);
		//}
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
			lc::draw_wire_aabb(node.aabb, vb);
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
