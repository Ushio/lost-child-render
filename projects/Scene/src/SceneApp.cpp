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
#include "helper_cinder/draw_camera.hpp"
#include "random_engine.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "collision_sphere.hpp"
#include "material.hpp"

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
	struct SphereObject {
		SphereObject(const Sphere &s, const Material &m) :sphere(s), material(m) {}
		Sphere sphere;
		Material material;
		bool use_importance = true;
	};

	struct TriangleMeshObject {
		std::string mesh;

		/* reconstruct member */
		BVH bvh;
	};

	struct ConelBoxObject {
		ConelBoxObject():ConelBoxObject(5.0){}
		ConelBoxObject(double size) {
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
			ColorTriangle() { }
			ColorTriangle(const Triangle &t, const Vec3 &c):triangle(t), color(c) {
			}
			Triangle triangle;
			Vec3 color;
		};
		std::vector<ColorTriangle> triangles;
	};
	struct ConelBoxIntersection : public Intersection {
		Vec3 intersect_normal() const {
			return triangle_normal(triangle.triangle, isback);
		}
		ConelBoxObject::ColorTriangle triangle;
		bool isback = false;
	};
	inline boost::optional<ConelBoxIntersection> intersect(const Ray &ray, const ConelBoxObject &b) {
		boost::optional<ConelBoxIntersection> r;
		for (int i = 0; i < b.triangles.size(); ++i) {
			if (auto intersection = intersect(ray, b.triangles[i].triangle)) {
				double tmin = r ? r->tmin : std::numeric_limits<double>::max();
				if (intersection->tmin < tmin) {
					ConelBoxIntersection newIntersection;
					newIntersection.tmin = intersection->tmin;
					newIntersection.isback = intersection->isback;
					newIntersection.triangle = b.triangles[i];
					r = newIntersection;
				}
			}
		}
		return r;
	}

	typedef boost::variant<SphereObject, ConelBoxObject> SceneObject;

	struct SphereObjectIntersection {
		const SphereObject *o = nullptr;
		SphereIntersection intersection;
	};
	struct ConelBoxObjectIntersection {
		const ConelBoxObject *o = nullptr;
		ConelBoxIntersection intersection;
	};
	typedef boost::variant<SphereObjectIntersection, ConelBoxObjectIntersection> ObjectIntersection;

	struct MicroSurfaceVisitor : public boost::static_visitor<MicroSurface> {
		MicroSurfaceVisitor(const Ray &r) :ray(r) {}
		MicroSurface operator()(const SphereObjectIntersection &intersection) {
			MicroSurface m;
			m.p = intersection.intersection.intersect_position(ray);
			m.n = intersection.intersection.intersect_normal(intersection.o->sphere.center, m.p);
			m.vn = m.n;
			m.m = intersection.o->material;
			m.isback = intersection.intersection.isback;
			return m;
		}
		MicroSurface operator()(const ConelBoxObjectIntersection &intersection) {
			MicroSurface m;
			m.p = intersection.intersection.intersect_position(ray);
			m.n = intersection.intersection.intersect_normal();
			m.vn = m.n;
			m.m = LambertMaterial(intersection.intersection.triangle.color);
			m.isback = intersection.intersection.isback;
			return m;
		}
		Ray ray;
	};

	struct ImportantArea {
		ImportantArea() {}
		ImportantArea(const Sphere &sphere) :shape(sphere) {}
		// ImportantArea(const AABB &aabb) :shape(aabb) {}

		Sphere shape;

		// boost::variant<Sphere, AABB> shape;

		/* p が重点サンプルの基準点 */
		template <class E>
		Sample<Vec3> sample(const Vec3 &p, RandomEngine<E> &engine) const {
			const double eps = 0.000001;

			double distance_pq;
			Vec3 q;
			Vec3 qn;
			do {
				q = shape.center + generate_on_sphere(engine) * shape.radius;
				qn = glm::normalize(q - shape.center);
				distance_pq = glm::distance(p, q);
			} while (distance_pq < eps);

			Sample<Vec3> s;
			s.value = q;
			double A = 4.0 * glm::pi<double>() * shape.radius * shape.radius;
			double cos_alpha = glm::abs(glm::dot(qn, (p - q) / distance_pq));
			if (cos_alpha < 0.0001) {
				s.pdf = 1.0 / eps;
			}
			else {
				s.pdf = distance_pq * distance_pq / (A * cos_alpha);
			}
			return s;
		}
	};

	struct Scene {
		Transform viewTransform;
		Camera camera;
		std::vector<SceneObject> objects;
		std::vector<ImportantArea> importances;
	};

	template <class E>
	Sample<Vec3> sample_important(const Scene &scene, const Vec3 &p, RandomEngine<E> &engine) {
		return scene.importances[engine() % scene.importances.size()].sample(p, engine);
	}

	inline boost::optional<MicroSurface> intersect(const Ray &ray, const Scene &scene) {
		double tmin = std::numeric_limits<double>::max();
		ObjectIntersection min_intersection;

		for (int i = 0; i < scene.objects.size(); ++i) {
			if (auto *s = boost::get<SphereObject>(&scene.objects[i])) {
				if (auto intersection = intersect(ray, s->sphere)) {
					if (intersection->tmin < tmin) {
						SphereObjectIntersection soi;
						soi.o = s;
						soi.intersection = *intersection;

						tmin = intersection->tmin;
						min_intersection = soi;
					}
				}
			}
			if (auto *c = boost::get<ConelBoxObject>(&scene.objects[i])) {
				if (auto intersection = intersect(ray, *c)) {
					if (intersection->tmin < tmin) {
						ConelBoxObjectIntersection soi;
						soi.o = c;
						soi.intersection = *intersection;

						tmin = intersection->tmin;
						min_intersection = soi;
					}
				}
			}
		}
		if (tmin != std::numeric_limits<double>::max()) {
			return min_intersection.apply_visitor(MicroSurfaceVisitor(ray));
		}
		return boost::none;
	}
	
	// 
	// direct samplingはオブジェクトごとに考えることにする

	// テクスチャ付きメッシュ
	//struct TexturedTriangleMeshObject {
	//	std::string mesh;
	//  std::string texture_albedo;
	//	BVH bvh;
	//};

	// TODO ダイレクトサンプリングの仕組み
}

// cinder
namespace lc {
	inline void draw_object(const ConelBoxObject &b) {
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

	inline void draw_scene(const Scene &scene, int image_width, int image_height) {
		for (int i = 0; i < scene.objects.size(); ++i) {
			scene.objects[i].apply_visitor(DrawObjectVisitor());
		}

		for (int i = 0; i < scene.importances.size(); ++i) {
			auto shape = scene.importances[i].shape;
			cinder::gl::ScopedPolygonMode wire(GL_LINE);
			cinder::gl::ScopedColor c(1.0, 0.3, 0.3);
			cinder::gl::drawSphere(shape.center, shape.radius, 20);
		}

		cinder::gl::ScopedMatrices smat;
		cinder::gl::ScopedColor c(0.5, 0.5, 0.5);
		cinder::gl::multModelMatrix(scene.viewTransform.inverse_matrix());
		lc::draw_camera(scene.camera, image_width, image_height, 10.0);
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

	lc::Vec3 eye(0.0, 0.0, 8.0);
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };

	lc::Camera::Settings camera_settings;
	camera_settings.fovy = glm::radians(60.0);
	scene.camera = lc::Camera(camera_settings);
	scene.viewTransform = lc::Transform(glm::lookAt(eye, look_at, up));

	scene.objects.push_back(lc::ConelBoxObject(5.0));
	scene.objects.push_back(lc::SphereObject(
		lc::Sphere(lc::Vec3(0.0), 1.0),
		lc::LambertMaterial(lc::Vec3(1.0))
	));

	scene.importances.push_back(lc::ImportantArea(lc::Sphere(lc::Vec3(0.0), 1.0)));
	// scene.importances.push_back(lc::ImportantArea(lc::Sphere(lc::Vec3(0.0, 2.0, 0.0), 1.0)));

	int wide = 512;
	lc::draw_scene(scene, wide, wide);

	// レイテスト
	lc::Xor xor;
	for (int i = 0; i < 100; ++i) {
		lc::Ray ray;

		if (i % 2 == 0) {
			ray = scene.camera.generate_ray(
				lc::generate_continuous(xor, 0.0, wide - 1),
				lc::generate_continuous(xor, 0.0, wide - 1),
				wide, wide);
			ray = scene.viewTransform.to_local_ray(ray);
		}
		else {
			auto s = lc::sample_important(scene, eye, xor);
			ray = lc::Ray(eye, glm::normalize(s.value - eye));

			// console() << "pdf: " << s.pdf << std::endl;
		}

		if (auto intersect = lc::intersect(ray, scene)) {
			gl::ScopedColor color(1.0f, 0.5f, 0.0f);
			gl::drawLine(ray.o, intersect->p);
			gl::drawSphere(intersect->p, 0.05f);

			auto r = glm::reflect(ray.d, intersect->n);
			gl::drawLine(intersect->p, intersect->p + r * 1.0);

			gl::ScopedColor color_2(0.8f, 0.8f, 0.0f);
			gl::drawLine(intersect->p, intersect->p + intersect->n * 0.5);
		}
		else {
			gl::ScopedColor color(0.0f, 0.0f, 0.5f);
			gl::drawLine(ray.o, ray.o + ray.d * 20.0);
		}
	}
}

CINDER_APP(SceneApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(false);
});
