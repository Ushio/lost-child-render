#pragma once

#include "render_type.hpp"
#include "parallel_for.hpp"

namespace lc {
	inline void gamma(Image &image, double g = 2.2) {
		auto p = Vec3(1.0 / 2.2);
		parallel_for(image.height, [p, &image](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				Vec3 *lineHead = image.pixels.data() + image.width * y;
				for (int x = 0; x < image.width; ++x) {
					Vec3 *rgb = lineHead + x;
					*rgb = glm::pow(*rgb, p);
				}
			}
		});
	}
}