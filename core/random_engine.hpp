#pragma once

#include <iostream>
#include <algorithm>
#include <inttypes.h>

#include <glm/glm.hpp>
#include "render_type.hpp"

namespace lc {
	template <class T>
	struct RandomEngine {
		uint32_t operator()() {
			return static_cast<T*>(this)->operator()();
		}
	};

	struct Xor128 : public RandomEngine<Xor128> {
		Xor128() {

		}
		Xor128(uint32_t seed) {
			_w = seed;
		}
		uint32_t _x = 123456789;
		uint32_t _y = 362436069;
		uint32_t _z = 521288629;
		uint32_t _w = 88675123;
		uint32_t operator()() {
			uint32_t t = _x ^ (_x << 11);
			_x = _y;
			_y = _z;
			_z = _w;
			return _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
		}
	};
	struct Xor : public RandomEngine<Xor> {
		Xor() {

		}
		Xor(uint32_t seed) {
			_y = seed;
		}
		uint32_t _y = 2463534242;
		uint32_t operator()() {
			_y = _y ^ (_y << 13); _y = _y ^ (_y >> 17);
			return _y = _y ^ (_y << 5);
		}
	};

	// [0, 1)
	template <class E>
	double generate_continuous(RandomEngine<E> &engine) {
		uint32_t uniform = engine();
		constexpr double c = 1.0 / static_cast<double>(0xffffffffLL + 1);
		return static_cast<double>(uniform) * c;
	}

	template <class T>
	struct Sample {
		T value;
		double pdf = 0.0;
	};

	template <class E> 
	Sample<Vec3> generate_cosine_weight_hemisphere(RandomEngine<E> &engine) {
		double eps_1 = generate_continuous(engine);
		double eps_2 = generate_continuous(engine);

		double theta = glm::acos(glm::sqrt(1.0 - eps_1));
		double phi = 2.0 * glm::pi<double>() * eps_2;
		double cos_theta = cos(theta);

		double z = sin(theta) * cos(phi);
		double x = sin(theta) * sin(phi);
		double y = cos_theta;

		Sample<Vec3> sample;
		sample.value = Vec3(x, y, z);
		sample.pdf = cos_theta / glm::pi<double>();
		return sample;
	}
}
