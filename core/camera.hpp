#pragma once

#include "render_type.hpp"
#include "transform.hpp"

namespace lc {
	// 基本は高さを基準に
	class Camera {
	public:
		struct Settings {
			double fovy = glm::radians(45.0);
		};
		Camera() {}
		Camera(const Settings &settings) :_settings(settings) {

		}

		/*
		image_x: 0 to (image_width - 1)
		image_y: 0 to (image_height - 1)
		*/
		Ray generate_ray(double image_x, double image_y, int image_width, int image_height) const {
			double aspect = double(image_width) / double(image_height);
			double top = glm::tan(_settings.fovy * 0.5);
			double bottom = -top;
			double right = top * aspect;
			double left = -right;

			Vec3 to(
				left + (right - left) * image_x / double(image_width - 1),
				bottom + (top - bottom) * image_y / double(image_height - 1),
				-1.0
			);

			return Ray(Vec3(), glm::normalize(to));
		}
	private:
		Settings _settings;
	};
}
