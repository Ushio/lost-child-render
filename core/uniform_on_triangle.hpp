#pragma once

#include <vector>
#include <algorithm>
#include "render_type.hpp"
#include "random_engine.hpp"
#include "triangle_area.hpp"

namespace lc {
	template <class Generator>
	inline Vec3 uniform_on_triangle(RandomEngine<Generator> &e, const Triangle &tri) {
		double eps1 = e.continuous();
		double eps2 = e.continuous();

		double sqrt_r1 = glm::sqrt(eps1);
		lc::Vec3 p =
			(1.0 - sqrt_r1) * tri[0]
			+ sqrt_r1 * (1.0 - eps2) * tri[1]
			+ sqrt_r1 * eps2 * tri[2];
		return p;
	}
	class UniformOnTriangle {
	public:
		void build() {
			if (_triangles.empty()) {
				_cumulative_areas.clear();
				_area = 0.0;
				return;
			}

			_cumulative_areas.resize(_triangles.size());
			double area = 0.0;
			for (int i = 0; i < _triangles.size(); ++i) {
				const Triangle &tri = _triangles[i];
				area += triangle_area(tri[0], tri[1], tri[2]);
				_cumulative_areas[i] = area;
			}
			_area = area;
		}

		struct Uniform {
			Vec3 p;
			int index = -1;
		};
		template <class Generator>
		Uniform uniform(RandomEngine<Generator> &e) const {
			double p = e.continuous(0.0, _area);
			auto it = std::upper_bound(_cumulative_areas.begin(), _cumulative_areas.end(), p);
			std::size_t index = std::distance(_cumulative_areas.begin(), it);
			index = std::min(index, _cumulative_areas.size() - 1);

			Uniform u;
			u.p = uniform_on_triangle(e, _triangles[index]);
			u.index = index;
			return u;
		}

		void set_triangle(const std::vector<Triangle> &triangles) {
			_triangles = triangles;
		}
		double get_area() const {
			return _area;
		}
		std::vector<Triangle> _triangles;
		std::vector<double> _cumulative_areas;
		double _area = 0.0;
	};
}
