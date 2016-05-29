#pragma once

#include "camera.hpp"
#include "cinder/gl/gl.h"

namespace lc {
	inline void draw_camera(const Camera &camera, int image_width, int image_height, double ray_length) {
		std::array<Ray, 4> rays = {
			camera.generate_ray(0.0, 0.0, image_width, image_height),
			camera.generate_ray(image_width - 1.0, 0.0, image_width, image_height),
			camera.generate_ray(image_width - 1.0, image_height - 1.0, image_width, image_height),
			camera.generate_ray(0.0, image_height - 1.0, image_width, image_height)
		};

		for (auto ray : rays) {
			cinder::gl::drawLine(ray.o, ray.o + ray.d * ray_length);
		}

		// split
		int N = 2;
		for (int i = 0; i < N; ++i) {
			std::array<Vec3, 4> ps;
			std::transform(rays.begin(), rays.end(), ps.begin(), [ray_length, i, N](const Ray &ray) {
				return ray.o + ray.d * ray_length * double(i + 1) / double(N);
			});
			for (int j = 0; j < ps.size(); ++j) {
				cinder::gl::drawLine(ps[j], ps[(j + 1) % ps.size()]);
			}
		}
	}
}