// rtcamp.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

//#include "stdafx.h"

#include <iostream>
#include <filesystem>
namespace fs = std::tr2::sys;

#include "render.hpp"

#include <windows.h>
#include <ppl.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
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

	lc::AccumlationBuffer *_buffer = nullptr;
	lc::Scene _scene;

	int wide = 256;
	_buffer = new lc::AccumlationBuffer(wide, wide);

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
	{
		auto light = lc::PolygonLight();
		light.emissive_front = lc::Vec3(5.0, 5.0, 0.5);
		light.emissive_back = lc::Vec3(0.5, 10.0, 10.0);

		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		
		std::string path = (exe_dir / "butterfly.obj").string();
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

		// デフォルトは奥を向いている？
		for (int i = 0; i < triangles.size(); ++i) {
			for (int j = 0; j < 3; ++j) {
				triangles[i][j] *= 10.0;
				triangles[i][j] = glm::rotateX(triangles[i][j], glm::radians(40.0));
				triangles[i][j] = glm::rotateY(triangles[i][j], glm::radians(70.0));
			}
		}

		light.uniform_triangle.set_triangle(triangles);
		light.uniform_triangle.build();
		light.bvh.set_triangle(triangles);
		light.bvh.build();

		_scene.add(light);
	}

	_scene.finalize();

	for (int i = 0; i < 50; ++i) {
		lc::step(*_buffer, _scene, 2);
	}
	std::string dst = (exe_dir / "render.png").string();
	write_as_png(dst, *_buffer);

    return 0;
}


