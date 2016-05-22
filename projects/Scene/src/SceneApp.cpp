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
#include "transform.hpp"

#include <stack>
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
#include <boost/format.hpp>


using namespace ci;
using namespace ci::app;
using namespace std;

class SceneApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;
	//cinder::TriMeshRef _mesh;
	//lc::BVH _bvh;
};

void SceneApp::setup()
{
	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 4.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 100.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(10.0f)).subdivisions(ivec2(10)), colorShader);

	//cinder::ObjLoader loader(loadAsset("bunny.obj"));
	//_mesh = cinder::TriMesh::create(loader);
	//_bvh.set_triangle(lc::to_triangles(_mesh));
	//_bvh.build();
}

void SceneApp::mouseDown(MouseEvent event)
{
}

void SceneApp::update()
{

}


void SceneApp::draw()
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


	ui::ScopedWindow window("Params", glm::vec2(200, 300));

}

CINDER_APP(SceneApp, RendererGl)
