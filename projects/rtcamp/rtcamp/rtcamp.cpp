// rtcamp.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

//#include "stdafx.h"

#include <iostream>
#include <fstream>
#include <future>

#include "render.hpp"
#include "image_processing.hpp"

#include <windows.h>
#include <ppl.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <boost/timer.hpp>
#include <boost/format.hpp>

static const double kRENDER_TIME = 60.0 * 5.0;
// static const double kRENDER_TIME = 30.0;
static const double kWRITE_INTERVAL = 20.0;
// static const int kSIZE = 1024;
// static const int kSIZE = 256;
static const int kSIZE = 1200;

static const double kNLM_TIME_ESTIMATE = 2.5; // 1200 x 1200
static const double kNLM_COEF = 0.35;

namespace {
	void write_as_png(std::string filename, const lc::Image &image) {
		std::vector<uint8_t> pixels(image.width * image.height * 3);
		parallel_for(image.height, [&pixels, &image](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				uint8_t *lineHead = pixels.data() + image.width * 3 * y;
				for (int x = 0; x < image.width; ++x) {
					int index = y * image.width + x;
					lc::Vec3 src = image.pixels[index];
					uint8_t *dstRGB = lineHead + x * 3;
					for (int i = 0; i < 3; ++i) {
						dstRGB[i] = static_cast<uint8_t>(glm::clamp(src[i], 0.0, 1.0) * 255.9999);
					}
				}
			}
		});

		stbi_write_png(filename.c_str(), image.width, image.height, 3, pixels.data(), image.width * 3);
	}

	//void write_as_png(std::string filename, const lc::AccumlationBuffer &buffer) {
	//	std::vector<uint8_t> pixels(buffer._width * buffer._height * 3);
	//	double normalize_value = 1.0 / buffer._iteration;

	//	concurrency::parallel_for<int>(0, buffer._height, [&buffer, &pixels, normalize_value](int y) {
	//		uint8_t *lineHead = pixels.data() + 3 * buffer._width * y;
	//		for (int x = 0; x < buffer._width; ++x) {
	//			int index = y * buffer._width + x;
	//			const lc::AccumlationBuffer::Pixel &pixel = buffer._data[index];
	//			lc::Vec3 color = pixel.color * normalize_value;
	//			lc::Vec3 gamma_corrected;
	//			for (int i = 0; i < 3; ++i) {
	//				gamma_corrected[i] = glm::pow(glm::clamp(color[i], 0.0, 1.0), 1.0 / 2.2);
	//			}
	//			uint8_t *dstRGB = lineHead + x * 3;
	//			for (int i = 0; i < 3; ++i) {
	//				dstRGB[i] = static_cast<uint8_t>(gamma_corrected[i] * 255.9999);
	//			}
	//		}
	//	});

	//	stbi_write_png(filename.c_str(), buffer._width, buffer._height, 3, pixels.data(), buffer._width * 3);
	//}
}

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

		std::vector<lc::Triangle> triangles_thorn;

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
			triangles_thorn.insert(triangles_thorn.end(), tris.begin(), tris.end());
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
			triangles_thorn.insert(triangles_thorn.end(), tris.begin(), tris.end());
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
			triangles_thorn.insert(triangles_thorn.end(), tris.begin(), tris.end());
		}

		mesh.bvh.set_triangle(triangles_thorn);
		mesh.bvh.build();
		scene.add(mesh);
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
int main(int argc, char *argv[])
{
	char name[1024];
	GetModuleFileNameA(nullptr, name, sizeof(name));

	auto exe_dir = lc::fs::path(name).parent_path();
	std::ofstream logstream(exe_dir / "log.txt");

#define LOG_LN(stream) logstream << stream << std::endl; std::cout << stream << std::endl;

	// http://patorjk.com/software/taag/#p=testall&f=Zodi&t=Lost%20Child%20Render
	LOG_LN(boost::format("_              _      ____ _     _ _     _   ____                _           "));
	LOG_LN(boost::format("| |    ___  ___| |_   / ___| |__ (_) | __| | |  _ \\ ___ _ __   __| | ___ _ __ "));
	LOG_LN(boost::format("| |   / _ \\/ __| __| | |   | '_ \\| | |/ _` | | |_) / _ \\ '_ \\ / _` |/ _ \\ '__|"));
	LOG_LN(boost::format("| |__| (_) \\__ \\ |_  | |___| | | | | | (_| | |  _ <  __/ | | | (_| |  __/ |   "));
	LOG_LN(boost::format("|_____\\___/|___/\\__|  \\____|_| |_|_|_|\\__,_| |_| \\_\\___|_| |_|\\__,_|\\___|_|   "));

	LOG_LN(boost::format(" "));
	LOG_LN(boost::format(" "));

	LOG_LN(boost::format("auther       : ushiostarfish"));
	LOG_LN(boost::format("mail         : ushiostarfish@gmail.com"));
	LOG_LN(boost::format("log          : log.txt"));
	LOG_LN(boost::format("render time  : %.2f") % kRENDER_TIME);
	LOG_LN(boost::format("save interval: %.2f") % kWRITE_INTERVAL);
	LOG_LN(boost::format("images       : render_###.png"));
	LOG_LN(boost::format("image size   : %d x %d") % kSIZE % kSIZE);

	LOG_LN(boost::format(" "));
	LOG_LN(boost::format("setup..."));

	boost::timer timer;
	boost::timer write_timer;

	lc::AccumlationBuffer *_buffer = nullptr;
	lc::Scene scene;

	_buffer = new lc::AccumlationBuffer(kSIZE, kSIZE);
	setup_scene(scene, exe_dir);
	
	LOG_LN(boost::format("initialized - %.2f s") % timer.elapsed());

	double step_time_sum = 0.0;

	lc::Image image;
	std::future<int> save_task = std::async(std::launch::async, []() { return 0; });

	int i = 0;
	for (i = 0; ; ++i) {
		boost::timer timer_step;

		lc::step(*_buffer, scene, 2);

		// あんまり書きすぎないように
		bool wrote = false;
		std::string name = boost::str(boost::format("render_%03d.png") % i);
		std::string dst = (exe_dir / name).string();
		if (i == 0 || kWRITE_INTERVAL < write_timer.elapsed()) {
			save_task.get();

			_buffer->to_image(image);

			save_task = std::async(std::launch::async, [&image, dst]() {
				lc::gamma(image);
				lc::color_correction(image);
				write_as_png(dst, image);
				return 0; 
			});

			write_timer.restart();

			wrote = true;
		}
		
		double elapsed = timer.elapsed();
		double step_elapsed = timer_step.elapsed();
		LOG_LN(boost::format("step[%d] - %.2f s (%.2f s)") % i % elapsed % step_elapsed);

		step_time_sum += step_elapsed;
		double avg = step_time_sum / (i + 1);

		// 次のステップを回したら、間に合わなそうならここで終了
		if (kRENDER_TIME - kNLM_TIME_ESTIMATE < elapsed + avg * 1.3) {
			break;
		}
	}

	// 最終フレーム処理
	save_task.get();

	_buffer->to_image(image);

	{
		lc::Image nlm_image;
		boost::timer timer_nlm;
		lc::non_local_means(nlm_image, image, kNLM_COEF);
		double elapsed_nlm = timer_nlm.elapsed();

		lc::gamma(nlm_image);
		lc::color_correction(nlm_image);

		std::string name = boost::str(boost::format("render_%03d_final.png") % i);
		std::string dst = (exe_dir / name).string();
		write_as_png(dst, nlm_image);

		LOG_LN(boost::format("nlm - %.2f s") % elapsed_nlm);
	}
	/*
	lc::Image nlm_image;
	for (int i = 0; i < 10; ++i) {
		double c = 0.05 * i;
		boost::timer timer_nlm;
		lc::non_local_means(nlm_image, image, c);
		double elapsed_nlm = timer_nlm.elapsed();

		lc::gamma(nlm_image);

		std::string name = boost::str(boost::format("nlm_%03d.png") % i);
		std::string dst = (exe_dir / name).string();
		write_as_png(dst, nlm_image);

		LOG_LN(boost::format("nlm - %.2f s") % elapsed_nlm);
	}
	*/
	double elapsed = timer.elapsed();
	LOG_LN(boost::format("done - %.2f s") % elapsed);

	while (timer.elapsed() < kRENDER_TIME) {
		Sleep(0);
	}

    return 0;
}


