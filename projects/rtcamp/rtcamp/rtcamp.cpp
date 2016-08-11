// rtcamp.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

//#include "stdafx.h"

#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::tr2::sys;

#include "render.hpp"

#include <windows.h>
#include <ppl.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <boost/timer.hpp>
#include <boost/format.hpp>

static const double kRENDER_TIME = 60.0 * 5.0;
static const int kSIZE = 1024;

namespace {
	void write_as_png(std::string filename, const lc::AccumlationBuffer &buffer) {
		std::vector<uint8_t> pixels(buffer._width * buffer._height * 3);
		double normalize_value = 1.0 / buffer._iteration;

		concurrency::parallel_for<int>(0, buffer._height, [&buffer, &pixels, normalize_value](int y) {
			uint8_t *lineHead = pixels.data() + 3 * buffer._width * y;
			for (int x = 0; x < buffer._width; ++x) {
				int index = y * buffer._width + x;
				const lc::AccumlationBuffer::Pixel &pixel = buffer._data[index];
				lc::Vec3 color = pixel.color * normalize_value;
				lc::Vec3 gamma_corrected;
				for (int i = 0; i < 3; ++i) {
					gamma_corrected[i] = glm::pow(glm::clamp(color[i], 0.0, 1.0), 1.0 / 2.2);
				}
				uint8_t *dstRGB = lineHead + x * 3;
				for (int i = 0; i < 3; ++i) {
					dstRGB[i] = static_cast<uint8_t>(gamma_corrected[i] * 255.9999);
				}
			}
		});

		stbi_write_png(filename.c_str(), buffer._width, buffer._height, 3, pixels.data(), buffer._width * 3);
	}
}

int main(int argc, char *argv[])
{
	char name[1024];
	GetModuleFileNameA(nullptr, name, sizeof(name));

	auto exe_dir = fs::path(name).parent_path();
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

	LOG_LN(boost::format("auther     : ushiostarfish"));
	LOG_LN(boost::format("mail       : ushiostarfish@gmail.com"));
	LOG_LN(boost::format("log        : log.txt"));
	LOG_LN(boost::format("render time: %.2f") % kRENDER_TIME);
	LOG_LN(boost::format("images     : render_###.png"));
	LOG_LN(boost::format("image size : %d x %d") % kSIZE % kSIZE);

	LOG_LN(boost::format(" "));
	LOG_LN(boost::format("setup..."));

	boost::timer timer;


	lc::AccumlationBuffer *_buffer = nullptr;
	lc::Scene _scene;

	_buffer = new lc::AccumlationBuffer(kSIZE, kSIZE);

	lc::Vec3 eye(0.0, 0.0, 60.0);
	lc::Vec3 look_at;
	lc::Vec3 up = { 0.0, 1.0, 0.0 };

	lc::Camera::Settings camera_settings;
	camera_settings.fovy = glm::radians(70.0);
	_scene.camera = lc::Camera(camera_settings);
	_scene.viewTransform = lc::Transform(glm::lookAt(eye, look_at, up));

	_scene.add(lc::ConelBoxObject(50.0));


	for (int i = 0; i < 5; ++i) {
		auto spec = lc::SphereObject(
			lc::Sphere(lc::Vec3(
				-20.0 + i * 10.0,
				-18.0,
				0.0), 5.0f),
			lc::CookTorranceMaterial(lc::Vec3(1.0), 0.1 + i * 0.2, 0.99 /*フレネル*/)
		);
		_scene.add(spec);
	}


	// ポリゴンライト
	std::string objpath = (exe_dir / "butterfly.obj").string();
	{
		auto light = lc::PolygonLight();
		light.emissive_front = lc::Vec3(3.0, 3.0, 0.5);
		light.emissive_back = lc::Vec3(0.5, 5.0, 5.0);

		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		bool ret = tinyobj::LoadObj(shapes, materials, err, objpath.c_str());

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

					tris[i][j] += lc::Vec3(-10.0, 0.0, 0.0);
				}
			}

			light.uniform_triangle.set_triangle(tris);
			light.uniform_triangle.build();
			light.bvh.set_triangle(tris);
			light.bvh.build();

			_scene.add(light);
		}

		{
			std::vector<lc::Triangle> tris = triangles;

			// デフォルトは奥を向いている？
			for (int i = 0; i < tris.size(); ++i) {
				for (int j = 0; j < 3; ++j) {
					tris[i][j] *= 10.0;
					tris[i][j] = glm::rotateX(tris[i][j], glm::radians(40.0));
					tris[i][j] = glm::rotateY(tris[i][j], glm::radians(-30.0));

					tris[i][j] += lc::Vec3(11.0, 10.0, -5.0);
				}
			}

			light.uniform_triangle.set_triangle(tris);
			light.uniform_triangle.build();
			light.bvh.set_triangle(tris);
			light.bvh.build();

			_scene.add(light);
		}
	}

	_scene.finalize();
	LOG_LN(boost::format("initialized - %.2f s") % timer.elapsed());

	double step_time_sum = 0.0;

	for (int i = 0; ; ++i) {
		boost::timer timer_step;

		lc::step(*_buffer, _scene, 2);

		std::string name = boost::str(boost::format("render_%03d.png") % i);
		std::string dst = (exe_dir / name).string();
		write_as_png(dst, *_buffer);
		
		double elapsed = timer.elapsed();
		double step_elapsed = timer_step.elapsed();
		LOG_LN(boost::format("step[%d] - %.2f s (%.2f s)") % i % elapsed % step_elapsed);

		step_time_sum += step_elapsed;
		double avg = step_time_sum / (i + 1);

		// 次のステップを回したら、間に合わなそうならここで終了
		if (kRENDER_TIME < elapsed + avg * 1.5) {
			break;
		}
	}

	double elapsed = timer.elapsed();
	LOG_LN(boost::format("done - %.2f s") % elapsed);

	while (timer.elapsed() < kRENDER_TIME) {
		Sleep(0);
	}

    return 0;
}


