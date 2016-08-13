#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"
#include "CinderImGui.h"

#include "render.hpp"

#include "helper_cinder/mesh_util.hpp"
#include "helper_cinder/draw_wire_aabb.hpp"
#include "helper_cinder/draw_camera.hpp"
#include "helper_cinder/draw_scene.hpp"

#include <stack>
#include <chrono>
#include <boost/format.hpp>
#include <boost/variant.hpp>

#include "fixed_vector.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

static const int wide = 256;

namespace lc {
	inline cinder::Surface32fRef to_surface(const AccumlationBuffer &buffer) {
		auto surface = cinder::Surface32f::create(buffer._width, buffer._height, false);
		double normalize_value = 1.0 / buffer._iteration;

		parallel_for(buffer._height, [&buffer, surface, normalize_value](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				float *lineHead = surface->getData(glm::ivec2(0, y));
				for (int x = 0; x < buffer._width; ++x) {
					int index = y * buffer._width + x;
					const AccumlationBuffer::Pixel &pixel = buffer._data[index];
					float *dstRGB = lineHead + x * 3;

					Vec3 color = pixel.color * normalize_value;

					for (int i = 0; i < 3; ++i) {
						dstRGB[i] = static_cast<float>(color[i]);
					}
				}
			}
		});
		/*concurrency::parallel_for<int>(0, buffer._height, [&buffer, surface, normalize_value](int y) {
			float *lineHead = surface->getData(glm::ivec2(0, y));
			for (int x = 0; x < buffer._width; ++x) {
				int index = y * buffer._width + x;
				const AccumlationBuffer::Pixel &pixel = buffer._data[index];
				float *dstRGB = lineHead + x * 3;

				Vec3 color = pixel.color * normalize_value;

				for (int i = 0; i < 3; ++i) {
					dstRGB[i] = static_cast<float>(color[i]);
				}
			}
		});*/
		return surface;
	}


	//// すこぶる微妙な気配
	//inline cinder::Surface32fRef median_filter(cinder::Surface32fRef image) {
	//	int width = image->getWidth();
	//	int height = image->getHeight();
	//	auto surface = cinder::Surface32f::create(width, height, false);
	//	float *lineHeadDst = surface->getData();
	//	float *lineHeadSrc = image->getData();
	//	concurrency::parallel_for<int>(0, height, [lineHeadDst, lineHeadSrc, width, height](int y) {
	//		for (int x = 0; x < width; ++x) {
	//			int index = y * width + x;
	//			struct Pix {
	//				Pix() {}
	//				Pix(const float *c):color(c[0], c[1], c[2]), luminance(0.299f*c[0] + 0.587f*c[1] + 0.114f*c[2]){
	//				}
	//				glm::vec3 color;
	//				float luminance;
	//			};

	//			std::array<Pix, 5> p3x3;
	//			//p3x3[0] = Pix(lineHeadSrc + (std::max(y - 1, 0) * width + std::max(x - 1, 0)) * 3);
	//			p3x3[0] = Pix(lineHeadSrc + (std::max(y - 1, 0) * width + x) * 3);
	//			//p3x3[2] = Pix(lineHeadSrc + (std::max(y - 1, 0) * width + std::min(x + 1, width - 1)) * 3);

	//			p3x3[1] = Pix(lineHeadSrc + (y * width + std::max(x - 1, 0)) * 3);
	//			p3x3[2] = Pix(lineHeadSrc + (y * width + x) * 3);
	//			p3x3[3] = Pix(lineHeadSrc + (y * width + std::min(x + 1, width - 1)) * 3);

	//			//p3x3[6] = Pix(lineHeadSrc + (std::min(y + 1, height - 1) * width + std::max(x - 1, 0)) * 3);
	//			p3x3[4] = Pix(lineHeadSrc + (std::min(y + 1, height - 1) * width + x) * 3);
	//			//p3x3[8] = Pix(lineHeadSrc + (std::min(y + 1, height - 1) * width + std::min(x + 1, width - 1)) * 3);
	//			
	//			std::nth_element(p3x3.begin(), p3x3.begin() + 2, p3x3.end(), [](const Pix &a, const Pix &b) { 
	//				return a.luminance < b.luminance;
	//			});

	//			Pix p = p3x3[2];
	//			float *dstRGB = lineHeadDst + (y * width + x) * 3;
	//			for (int i = 0; i < 3; ++i) {
	//				dstRGB[i] = p.color[i];
	//			}
	//		}
	//	});
	//	return surface;
	//}
}

using namespace ci;
using namespace ci::app;
using namespace std;

inline void setup_scene(lc::Scene &scene, lc::fs::path asset_path) {

	lc::Vec3 eye(0.0, 0.0, 60.0);
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };

	lc::Camera::Settings camera_settings;
	camera_settings.fovy = glm::radians(70.0);
	scene.camera = lc::Camera(camera_settings);
	scene.viewTransform = lc::Transform(glm::lookAt(eye, look_at, up));

	// scene.add(lc::ConelBoxObject(50.0));

	//auto spec = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(-10.0, -15, -10.0), 10.0),
	//	lc::PerfectSpecularMaterial()
	//);
	//scene.add(spec);

	// 質感テスト
	//for (int i = 0; i < 5; ++i) {
	//	auto spec = lc::SphereObject(
	//		lc::Sphere(lc::Vec3(
	//			-20.0 + i * 10.0,
	//			-18.0,
	//			0.0), 5.0f),
	//		lc::CookTorranceMaterial(lc::Vec3(1.0), 0.1 + i * 0.2, 0.99 /*フレネル*/)
	//	);
	//	scene.add(spec);
	//}

	//auto spec = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(0.0, -10, 0.0), 10.0),
	//	lc::CookTorranceMaterial(lc::Vec3(1.0), 0.01, 0.01f /*フレネル*/)
	//);
	//scene.add(spec);

	//auto spec = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(-10.0, -15, 10.0), 10.0),
	//	lc::CookTorranceMaterial(lc::Vec3(0.0), lc::Vec3(1.0), 0.01, 0.05f /*フレネル*/)
	//);
	// scene.add(spec);

	//auto grass = lc::SphereObject(
	//	lc::Sphere(lc::Vec3(10.0, -10, 10.0), 7.0),
	//	lc::RefractionMaterial(1.4)
	//);
	//scene.add(grass);

	//cinder::ObjLoader loader(loadAsset("dragon.obj"));
	//auto mesh = cinder::TriMesh::create(loader);

	// TODO scaleバグ

	// auto mesh = cinder::TriMesh::create(cinder::geom::Sphere().radius(10.0));

	/*auto dragon = lc::TriangleMeshObject();
	dragon.bvh.set_triangle(lc::to_triangles(mesh));
	for (int i = 0; i < dragon.bvh._triangles.size(); ++i) {
	for (int j = 0; j < 3; ++j) {
	dragon.bvh._triangles[i].v[j] *= 30.0;
	}
	}
	dragon.bvh.build();
	lc::Mat4 dragonMat;
	dragonMat = glm::translate(dragonMat, lc::Vec3(0.0, -20.0, 0.0));
	dragon.transform = lc::Transform(dragonMat);
	dragon.material = lc::RefractionMaterial(1.4);
	scene.objects.push_back(dragon);*/

	// でかいライト
	{
		auto light = lc::DiscLight();
		light.disc = lc::make_disc(
			lc::Vec3(0.0, 50.0, 30.0),
			glm::normalize(lc::Vec3(0.0, -1.0, -0.8)),
			40.0
		);
		light.emissive = lc::EmissiveMaterial(lc::Vec3(1.5));
		scene.add(light);
	}

	//{
	//	auto light = lc::DiscLight();
	//	light.disc = lc::make_disc(
	//		lc::Vec3(0.0, 24.0, 0.0),
	//		lc::Vec3(0.0, -1.0, 0.0),
	//		7
	//	);
	//	light.emissive = lc::EmissiveMaterial(lc::Vec3(5.0));
	//	light.doubleSided = false;
	//	scene.add(light);
	//}
	//{
	//	auto light = lc::DiscLight();
	//	light.disc = lc::make_disc(
	//		lc::Vec3(20.0f, 10.0, 0.0),
	//		glm::normalize(lc::Vec3(-1.0, -1.0, 0.0)),
	//		5
	//	);
	//	light.emissive = lc::EmissiveMaterial(lc::Vec3(10.0));
	//	light.doubleSided = false;
	//	scene.add(light);
	//}
	//{
	//	auto light = lc::DiscLight();
	//	light.disc = lc::make_disc(
	//		lc::Vec3(-20.0f, 10.0, 0.0),
	//		glm::normalize(lc::Vec3(1.0, -1.0, 0.0)),
	//		5
	//	);
	//	light.emissive = lc::EmissiveMaterial(lc::Vec3(10.0));
	//	light.doubleSided = false;
	//	scene.add(light);
	//}


	// テストはと
	{
		double scale_value = 2.0;
		lc::Mat4 transform;
		transform = glm::translate(transform, lc::Vec3(-20.0, -25.0, 15.0));
		transform = glm::rotate(transform, glm::radians(15.0), lc::Vec3(0.0, 1.0, 0.0));
		transform = glm::scale(transform, lc::Vec3(scale_value));

		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		std::string path = (asset_path / "hato.obj").string();
		bool ret = tinyobj::LoadObj(shapes, materials, err, path.c_str());

		for (int k = 0; k < shapes.size(); ++k) {
			const tinyobj::shape_t &shape = shapes[k];
			auto mesh = lc::MeshObject();
			if (shape.name == "hato.002_hato.003") {
				mesh.material = lc::LambertMaterial(lc::Vec3(0.4, 0.9, 0.95));
			}
			else {
				mesh.material = lc::LambertMaterial(lc::Vec3(0.85, 0.63, 0.85));
			}

			std::vector<lc::Triangle> triangles;
			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
				lc::Triangle tri;
				for (int j = 0; j < 3; ++j) {
					int idx = shape.mesh.indices[i + j];
					for (int k = 0; k < 3; ++k) {
						tri.v[j][k] = shape.mesh.positions[idx * 3 + k];
					}
				}
				triangles.push_back(tri);
			}
			std::vector<lc::Triangle> tris = triangles;

			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] = lc::mul3x4(transform, tris[i][j]);
				}
			}
			mesh.bvh.set_triangle(tris);
			mesh.bvh.build();

			scene.add(mesh);
		}

		std::array<lc::Vec3, 2> eyes = {
			lc::Vec3(1.405, 4.02, 0.594),
			lc::Vec3(1.405, 4.02, -0.594)
		};
		for (int i = 0; i < eyes.size(); ++i) {
			auto eye = lc::SphereObject(
				lc::Sphere(lc::mul3x4(transform, eyes[i]), 0.15 * scale_value),
				lc::CookTorranceMaterial(lc::Vec3(0.0), lc::Vec3(1.0), 0.01, 0.05 /*フレネル*/)
			);
			scene.add(eye);
		}
	}

	{
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		std::string path = (asset_path / "rose.obj").string();
		bool ret = tinyobj::LoadObj(shapes, materials, err, path.c_str());


		std::vector<lc::Triangle> triangles;
		const tinyobj::shape_t &shape = shapes[0];
		for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
			lc::Triangle tri;
			for (int j = 0; j < 3; ++j) {
				int idx = shape.mesh.indices[i + j];
				for (int k = 0; k < 3; ++k) {
					tri.v[j][k] = shape.mesh.positions[idx * 3 + k];
				}
			}
			triangles.push_back(tri);
		}

		std::array<lc::Vec3, 3> positions = {
			lc::Vec3(0.0,  -25.0, 10.0),
			lc::Vec3(-25.0, -25.0, -15.0),
			lc::Vec3(30.0, -25.0, -25.0),
		};
		std::array<lc::Vec3, 3> colors = {
			lc::Vec3(0.98, 0.13, 0.35),
			lc::Vec3(0.98, 0.9, 0.35),
			lc::Vec3(0.35, 0.13, 0.98),
		};
		for (int ri = 0; ri < 3; ++ri) {
			std::vector<lc::Triangle> tris = triangles;

			lc::Mat4 transform;
			transform = glm::translate(transform, positions[ri]);
			transform = glm::scale(transform, lc::Vec3(9.0));

			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] = lc::mul3x4(transform, tris[i][j]);
				}
			}

			auto mesh = lc::MeshObject();
			mesh.material = lc::LambertMaterial(colors[ri]);
			mesh.bvh.set_triangle(tris);
			mesh.bvh.build();

			scene.add(mesh);
		}
	}

	{
		lc::Mat4 transform;
		transform = glm::translate(transform, lc::Vec3(0.0, -25.0, 30.0));
		transform = glm::scale(transform, lc::Vec3(120.0));

		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		std::string path = (asset_path / "floor.obj").string();
		bool ret = tinyobj::LoadObj(shapes, materials, err, path.c_str());

		for (int k = 0; k < shapes.size(); ++k) {
			const tinyobj::shape_t &shape = shapes[k];
			auto mesh = lc::MeshObject();
			// mesh.material = lc::CookTorranceMaterial(lc::Vec3(1.0), 0.4, 0.99);
			// mesh.material = lc::PerfectSpecularMaterial();
			mesh.material = lc::LambertMaterial(lc::Vec3(1.0));

			std::vector<lc::Triangle> triangles;
			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
				lc::Triangle tri;
				for (int j = 0; j < 3; ++j) {
					int idx = shape.mesh.indices[i + j];
					for (int k = 0; k < 3; ++k) {
						tri.v[j][k] = shape.mesh.positions[idx * 3 + k];
					}
				}
				triangles.push_back(tri);
			}
			std::vector<lc::Triangle> tris = triangles;

			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] = lc::mul3x4(transform, tris[i][j]);
				}
			}
			mesh.bvh.set_triangle(tris);
			mesh.bvh.build();

			scene.add(mesh);
		}
	}

	{
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		std::string path = (asset_path / "thorn_c.obj").string();
		bool ret = tinyobj::LoadObj(shapes, materials, err, path.c_str());

		std::vector<lc::Triangle> triangles;
		auto mesh = lc::MeshObject();
		mesh.material = lc::CookTorranceMaterial(lc::Vec3(0.3, 0.7, 0.2), 0.4, 0.99);

		for (int k = 0; k < shapes.size(); ++k) {
			const tinyobj::shape_t &shape = shapes[k];

			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
				lc::Triangle tri;
				for (int j = 0; j < 3; ++j) {
					int idx = shape.mesh.indices[i + j];
					for (int k = 0; k < 3; ++k) {
						tri.v[j][k] = shape.mesh.positions[idx * 3 + k];
					}
				}
				triangles.push_back(tri);
			}
		}

		{
			auto tris = triangles;
			lc::Mat4 transform;
			transform = glm::rotate(transform, glm::radians(40.0), lc::Vec3(0.0, -1.0, 1.0));
			transform = glm::translate(transform, lc::Vec3(0.0, -20.0, -10.0));
			transform = glm::scale(transform, lc::Vec3(200.0));

			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] = lc::mul3x4(transform, tris[i][j]);
				}
			}
			mesh.bvh.set_triangle(tris);
			mesh.bvh.build();
			scene.add(mesh);
		}

		{
			auto tris = triangles;
			lc::Mat4 transform;
			transform = glm::rotate(transform, glm::radians(20.0), lc::Vec3(0.0, 1.0, 1.0));
			transform = glm::translate(transform, lc::Vec3(20.0, -40.0, -10.0));
			transform = glm::scale(transform, lc::Vec3(200.0));

			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] = lc::mul3x4(transform, tris[i][j]);
				}
			}
			mesh.bvh.set_triangle(tris);
			mesh.bvh.build();
			scene.add(mesh);
		}
		{
			auto tris = triangles;
			lc::Mat4 transform;
			transform = glm::rotate(transform, glm::radians(20.0), lc::Vec3(0.0, 1.0, -1.0));
			transform = glm::translate(transform, lc::Vec3(-20.0, -40.0, -10.0));
			transform = glm::scale(transform, lc::Vec3(200.0));

			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] = lc::mul3x4(transform, tris[i][j]);
				}
			}
			mesh.bvh.set_triangle(tris);
			mesh.bvh.build();
			scene.add(mesh);
		}

	}

	// ポリゴンライト
	{
		const double kLightPower = 55.0;

		auto light = lc::PolygonLight();
		//light.emissive_front = lc::Vec3(3.0, 3.0, 0.5);
		//light.emissive_back = lc::Vec3(0.5, 5.0, 5.0);
		// light.emissive_front = light.emissive_back = lc::Vec3(20.0, 10.0, 10.0);

		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		std::string path = (asset_path / "butterfly.obj").string();
		bool ret = tinyobj::LoadObj(shapes, materials, err, path.c_str());

		const tinyobj::shape_t &shape = shapes[0];
		std::vector<lc::Triangle> triangles;
		for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
			lc::Triangle tri;
			for (int j = 0; j < 3; ++j) {
				int idx = shape.mesh.indices[i + j];
				for (int k = 0; k < 3; ++k) {
					tri.v[j][k] = shape.mesh.positions[idx * 3 + k];
				}
			}
			triangles.push_back(tri);
		}

		{
			std::vector<lc::Triangle> tris = triangles;

			// デフォルトは奥を向いている？
			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] *= 15.0;
					tris[i][j] = glm::rotateX(tris[i][j], glm::radians(40.0));
					tris[i][j] = glm::rotateY(tris[i][j], glm::radians(70.0));

					tris[i][j] += lc::Vec3(-15.0, 5.0, 0.0);
				}
			}

			light.uniform_triangle.set_triangle(tris);
			light.uniform_triangle.build();
			light.bvh.set_triangle(tris);
			light.bvh.build();

			light.emissive_front = light.emissive_back = lc::Vec3(kLightPower * 0.5, kLightPower * 0.5, 0.9);

			scene.add(light);
		}

		{
			std::vector<lc::Triangle> tris = triangles;

			// デフォルトは奥を向いている？
			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] *= 10.0;
					tris[i][j] = glm::rotateX(tris[i][j], glm::radians(40.0));
					tris[i][j] = glm::rotateY(tris[i][j], glm::radians(-30.0));

					tris[i][j] += lc::Vec3(20.0, 15.0, -5.0);
				}
			}

			light.uniform_triangle.set_triangle(tris);
			light.uniform_triangle.build();
			light.bvh.set_triangle(tris);
			light.bvh.build();

			light.emissive_front = light.emissive_back = lc::Vec3(kLightPower, 0.9, kLightPower);

			scene.add(light);
		}

		{
			std::vector<lc::Triangle> tris = triangles;

			// デフォルトは奥を向いている？
			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] *= 10.0;
					tris[i][j] = glm::rotateX(tris[i][j], glm::radians(20.0));
					tris[i][j] = glm::rotateY(tris[i][j], glm::radians(-45.0));

					tris[i][j] += lc::Vec3(-15.0, 25.0, -15.0);
				}
			}

			light.uniform_triangle.set_triangle(tris);
			light.uniform_triangle.build();
			light.bvh.set_triangle(tris);
			light.bvh.build();

			light.emissive_front = light.emissive_back = lc::Vec3(0.9, kLightPower, kLightPower);

			scene.add(light);
		}
	}

	scene.finalize();
}

class RayTracerApp : public App {
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	void write_exr(const char *name, cinder::Surface32fRef surface) {
		auto dstPath = getAssetPath("") / name;
		writeImage(dstPath, *surface, ImageTarget::Options().quality(1.0f), "");
	}

	CameraPersp	_camera;
	CameraUi _cameraUi;
	gl::BatchRef _plane;

	lc::AccumlationBuffer *_buffer = nullptr;
	lc::Scene _scene;
	cinder::Surface32fRef _surface;
	gl::Texture2dRef _texture;
	gl::FboRef _fbo;
	gl::GlslProgRef _previewShader;

	double _renderTime = 0.0;

	bool _render = false;
	float _previewScale = 1.0f;
	float _previewGamma = 2.2f;
	bool _median = false;
};



void RayTracerApp::setup()
{
	_controlfp_s(NULL, _EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INEXACT, _MCW_EM);

	lc::fixed_vector<std::string, 5> tst;
	tst.push_back("a");
	tst.push_back("a");
	tst.pop_back();
	tst.push_back("c");
	for (auto p : tst) {
		std::cout << p << std::endl;
	}

	ui::initialize();

	_camera.lookAt(vec3(0, 0.0f, 60.0f), vec3(0.0f));
	_camera.setPerspective(40.0f, getWindowAspectRatio(), 0.01f, 500.0f);
	_cameraUi = CameraUi(&_camera, getWindow());

	auto colorShader = gl::getStockShader(gl::ShaderDef().color());
	_plane = gl::Batch::create(geom::WirePlane().size(vec2(100.0f)).subdivisions(ivec2(10)), colorShader);

	//cinder::ObjLoader loader(loadAsset("bunny.obj"));
	//_mesh = cinder::TriMesh::create(loader);
	//_bvh.set_triangle(lc::to_triangles(_mesh));
	//_bvh.build();

	gl::Fbo::Format format = gl::Fbo::Format()
		.colorTexture()
		.disableDepth();
	_fbo = gl::Fbo::create(wide, wide, format);
	{
		gl::ScopedFramebuffer fb(_fbo);
		gl::ScopedViewport vp(ivec2(0), _fbo->getSize());
		gl::ScopedMatrices m;
		gl::clear(Color(0.25, 0.25, 0.25));
	}
	_previewShader = gl::GlslProg::create(gl::GlslProg::Format()
		.vertex(loadAsset("preview_shader/shader.vert"))
		.fragment(loadAsset("preview_shader/shader.frag")));

	_buffer = new lc::AccumlationBuffer(wide, wide);

	setup_scene(_scene, getAssetPath(""));
}

void RayTracerApp::mouseDown(MouseEvent event)
{
}

void RayTracerApp::update()
{
}


void RayTracerApp::draw()
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

	lc::Path path;
	if (_render == false) {
		lc::draw_scene(_scene, wide, wide);

		auto ray_view = _scene.camera.generate_ray(_buffer->_width * 0.5, _buffer->_height * 0.5, _buffer->_width, _buffer->_height);

		gl::ScopedColor color(1.0, 0.5, 0.0);

		auto ray = _scene.viewTransform.to_local_ray(ray_view);

		
		//if (path.intersections.empty() == false) {
		//	gl::drawLine(ray.o, path.intersections[0].p);
		//	for (int i = 0; i < path.intersections.size() - 1; ++i) {
		//		gl::drawLine(path.intersections[i].p, path.intersections[i + 1].p);
		//	}
		//}
	}

	const int aa = 2;
	bool fbo_update = false;
	
	ui::ScopedWindow window("Params", glm::vec2(200, 300));
	ui::Text("samples: %d", _buffer->_iteration);
	ui::Text("time: %.2f s", _renderTime);
	ui::Text("rays per miliseconds: %.2f", aa * _buffer->_width * _buffer->_height * _buffer->_iteration * 0.001 / (_renderTime + 0.0001));
	
	ui::Checkbox("render", &_render);

	if (_render) {
		double beg = getElapsedSeconds();
		lc::step(*_buffer, _scene, aa);
		double duration = getElapsedSeconds() - beg;

		_renderTime += duration;

		//if (_median) {
		//	_surface = lc::median_filter(lc::to_surface(*_buffer));
		//} else {
		//	_surface = lc::to_surface(*_buffer);
		//}
		_surface = lc::to_surface(*_buffer);
		_texture = gl::Texture2d::create(*_surface);

		fbo_update = true;
	}
	if (ui::SliderFloat("preview scale", &_previewScale, 0.0f, 5.0f)) {
		fbo_update = true;
	}
	if (ui::SliderFloat("preview gamma", &_previewGamma, 0.0f, 4.0f)) {
		fbo_update = true;
	}
	// ui::Checkbox("median", &_median);
	if (ui::Button("save")) {
		write_exr("image.exr", _surface);
	}
	
	
	if (fbo_update && _texture) {
		gl::ScopedFramebuffer fb(_fbo);
		gl::ScopedViewport vp(ivec2(0), _fbo->getSize());
		gl::ScopedMatrices m;
		gl::setMatricesWindow(_fbo->getSize());

		gl::ScopedGlslProg shaderScp(_previewShader);
		gl::ScopedTextureBind texBindScp(_texture);
		_previewShader->uniform("u_scale", _previewScale);
		_previewShader->uniform("u_gamma_correction", 1.0f / _previewGamma);
		gl::drawSolidRect(_texture->getBounds());
	}

	ui::Image(_fbo->getTexture2d(GL_COLOR_ATTACHMENT0), ImVec2(_fbo->getWidth(), _fbo->getHeight()));
}

CINDER_APP(RayTracerApp, RendererGl, [](App::Settings *settings) {
	settings->setConsoleWindowEnabled(true);
});
