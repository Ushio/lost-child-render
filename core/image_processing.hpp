#pragma once

#include "render_type.hpp"
#include "parallel_for.hpp"

namespace lc {
	// マニュアルトーンマッピング
	inline void tone_mapping(Image &image) {
		parallel_for(image.height, [&image](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				Vec3 *lineHead = image.pixels.data() + image.width * y;
				for (int x = 0; x < image.width; ++x) {
					Vec3 &rgb = lineHead[x];
					for (int i = 0; i < 3; ++i) {
						rgb[i] = glm::two_over_pi<double>() * glm::atan(2.4 * glm::two_over_pi<double>() * rgb[i]);
					}
				}
			}
		});
	}

	inline void gamma(Image &image, double g = 2.2) {
		auto p = 1.0 / 2.2;
		parallel_for(image.height, [p, &image](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				Vec3 *lineHead = image.pixels.data() + image.width * y;
				for (int x = 0; x < image.width; ++x) {
					Vec3 &rgb = lineHead[x];
					for (int i = 0; i < 3; ++i) {
						rgb[i] = glm::pow(rgb[i], p);
					}
				}
			}
		});
	}

	// コントラスト調整
	inline void contrast(Image &image, double c) {
		parallel_for(image.height, [&image, c](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				Vec3 *lineHead = image.pixels.data() + image.width * y;
				for (int x = 0; x < image.width; ++x) {
					Vec3 &rgb = lineHead[x];
					for (int i = 0; i < 3; ++i) {
						rgb[i] = (rgb[i] - 0.5) * c + 0.5;
					}
				}
			}
		});
	}

	inline void non_local_means(Image &image_dst, const Image &image_src, double coef) {
		double param_h = std::max(0.0001, coef);
		double sigma = std::max(0.0001, coef);
		double frac_param_h_squared = 1.0 / (param_h * param_h);
		double sigma_squared = sigma * sigma;

		image_dst.resize(image_src.width, image_src.height);

		const int kKernel = 5;
		const int kSupport = 13;
		const int kHalfKernel = kKernel / 2;
		const int kHalfSupport = kSupport / 2;

		int width = image_src.width;
		int height = image_src.height;
		const double *src = glm::value_ptr(*image_src.pixels.data());
		double *dst = glm::value_ptr(*image_dst.pixels.data());

		typedef std::array<double, 3 * kKernel * kKernel> Template;

		auto sample_template = [width, height, src, kHalfKernel](int x, int y) {
			Template t;
			int index = 0;
			for (int sx = x - kHalfKernel; sx <= x + kHalfKernel; ++sx) {
				for (int sy = y - kHalfKernel; sy <= y + kHalfKernel; ++sy) {
					int sample_x = sx;
					int sample_y = sy;
					sample_x = std::max(sample_x, 0);
					sample_x = std::min(sample_x, width - 1);
					sample_y = std::max(sample_y, 0);
					sample_y = std::min(sample_y, height - 1);

					const double *p = src + (sample_y * width + sample_x) * 3;
					for (int i = 0; i < 3; ++i) {
						t[index++] = p[i];
					}
				}
			}
			return t;
		};
		auto distance_sqared_template = [](const Template &a, const Template &b) {
			double accum = 0.0;
			for (int i = 0; i < a.size(); ++i) {
				accum += glm::pow(a[i] - b[i], 2);
			}
			return accum;
		};
		parallel_for(height, [=](int beg_y, int end_y) {
			for (int y = beg_y; y < end_y; ++y) {
				for (int x = 0; x < width; ++x) {
					auto sample = [=](int x, int y) {
						int sample_x = x;
						int sample_y = y;
						sample_x = std::max(sample_x, 0);
						sample_x = std::min(sample_x, width - 1);
						sample_y = std::max(sample_y, 0);
						sample_y = std::min(sample_y, height - 1);

						const double *p = src + (sample_y * width + sample_x) * 3;
						return Vec3(p[0], p[1], p[2]);
					};

					double *dst_pixel = dst + (y * width + x) * 3;

					auto focus = sample_template(x, y);

					Vec3 sum;
					double sum_weight = 0.0;
					for (int sx = x - kHalfSupport; sx <= x + kHalfSupport; ++sx) {
						for (int sy = y - kHalfSupport; sy <= y + kHalfSupport; ++sy) {
							auto target = sample_template(sx, sy);
							auto dist = distance_sqared_template(focus, target);
							auto arg = -glm::max(dist - 2.0 * sigma_squared, 0.0) * frac_param_h_squared;
							auto weight = glm::exp(arg);

							sum_weight += weight;
							sum += sample(sx, sy) * weight;
						}
					}
					auto color = sum / sum_weight;

					for (int i = 0; i < 3; ++i) {
						dst_pixel[i] = color[i];
					}
				}
			}
		});
	}
}