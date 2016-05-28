#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#define LC_USE_STD_FILESYSTEM

#include "collision_triangle.hpp"
#include "bvh.hpp"
#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"

#include <stack>
#include <chrono>
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
#include <boost/format.hpp>
#include <boost/variant.hpp>

/*
	シーン
*/
namespace lc {
	struct LambertMaterial {
		LambertMaterial() {}
		LambertMaterial(Vec3 albedo_) :albedo(albedo_) {}

		Vec3 albedo = Vec3(1.0);
	};
	struct EmissiveMaterial {
		EmissiveMaterial() {}
		EmissiveMaterial(Vec3 color_) :color(color_) {}

		Vec3 color = Vec3(1.0);
	};

	typedef boost::variant<EmissiveMaterial, LambertMaterial> Material;

	struct MicroSurface {
		Vec3 p;
		Vec3 n;
		Vec3 vn; /* virtual normal */	
		bool isback = false;
		Material m;
	};

	struct SphereObject {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}
		Sphere sphere;
		Material material;
	};

	struct TriangleMeshObject {
		std::string mesh;

		/* reconstruct member */
		BVH bvh;
	};

	struct ConelBox {
		ConelBox():ConelBox(5.0){}
		ConelBox(double size) {
			double hsize = size * 0.5;
			Vec3 R(1.0, 0.0, 0.0);
			Vec3 G(0.0, 1.0, 0.0);
			Vec3 W(1.0);

			Vec3 ps[] = {
				Vec3(-hsize, hsize, -hsize),
				Vec3(-hsize, hsize, hsize),
				Vec3(hsize, hsize, hsize),
				Vec3(hsize, hsize, -hsize),

				Vec3(-hsize, -hsize, -hsize),
				Vec3(-hsize, -hsize, hsize),
				Vec3(hsize, -hsize, hsize),
				Vec3(hsize, -hsize, -hsize)
			};

			triangles.emplace_back(Triangle(ps[0], ps[1], ps[4]), R);
			triangles.emplace_back(Triangle(ps[1], ps[5], ps[4]), R);

			triangles.emplace_back(Triangle(ps[2], ps[3], ps[7]), G);
			triangles.emplace_back(Triangle(ps[6], ps[2], ps[7]), G);

			triangles.emplace_back(Triangle(ps[0], ps[1], ps[2]), W);
			triangles.emplace_back(Triangle(ps[0], ps[2], ps[3]), W);

			triangles.emplace_back(Triangle(ps[0], ps[4], ps[7]), W);
			triangles.emplace_back(Triangle(ps[3], ps[0], ps[7]), W);

			triangles.emplace_back(Triangle(ps[4], ps[5], ps[6]), W);
			triangles.emplace_back(Triangle(ps[6], ps[7], ps[4]), W);
		}

		struct ColorTriangle {
			ColorTriangle(const Triangle &t, const Vec3 &c):triangle(t), color(c) {
			}
			Triangle triangle;
			Vec3 color;
		};
		std::vector<ColorTriangle> triangles;
	};

	typedef boost::variant<SphereObject, ConelBox> SceneObject;

	struct Scene {
		Scene() {
			
		}

		Transform viewTransform;
		Camera camera;
		std::vector<SceneObject> objects;
	};

	// 
	// direct samplingはオブジェクトごとに考えることにする

	// テクスチャ付きメッシュ
	//struct TexturedTriangleMeshObject {
	//	std::string mesh;
	//  std::string texture_albedo;
	//	BVH bvh;
	//};
}

// cinder
namespace lc {
	inline void draw_object(const ConelBox &b) {
		cinder::gl::VertBatch vb_tri(GL_TRIANGLES);

		for (auto tri : b.triangles) {
			vb_tri.color(tri.color.r, tri.color.g, tri.color.b);
			for (int i = 0; i < 3; ++i) {
				vb_tri.vertex(tri.triangle.v[i]);
			}
		}

		vb_tri.draw();
	}
	inline void draw_object(const SphereObject &o) {
		cinder::gl::ScopedPolygonMode wire(GL_LINE);
		cinder::gl::ScopedColor c(0.5, 0.5, 0.5);
		cinder::gl::drawSphere(o.sphere.center, o.sphere.radius, 15);
	}

	struct DrawObjectVisitor : public boost::static_visitor<> {
		template <class T>
		void operator() (const T &o) {
			draw_object(o);
		}
	};

	inline void draw_scene(const Scene &scene) {
		for (int i = 0; i < scene.objects.size(); ++i) {
			scene.objects[i].apply_visitor(DrawObjectVisitor());
		}
	}
}


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

	lc::Scene scene;

	_camera.lookAt(vec3(0, 0.0f, 6.0f), vec3(0.0f));
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
	
	lc::Scene scene;

	scene.objects.push_back(lc::ConelBox(5.0));
	scene.objects.push_back(lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0), 1.0),
		lc::LambertMaterial(lc::Vec3(1.0))
	));

	lc::draw_scene(scene);
}

CINDER_APP(SceneApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(true);
});
