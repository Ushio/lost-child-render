#pragma once

//#include <iostream>
//#include <algorithm>
//
//#include <inttypes.h>
//
//#include <glm/glm.hpp>
//#include <boost/random.hpp>
//

#include <inttypes.h>
#include <math.h>
#include <algorithm>
#include <random>

#include <glm/glm.hpp>

#include "render_type.hpp"

namespace lc {
	struct Xor {
		Xor() {

		}
		Xor(uint32_t seed) {
			_y = std::max(seed, 1u);
		}
		uint32_t generate() {
			_y = _y ^ (_y << 13); _y = _y ^ (_y >> 17);
			return _y = _y ^ (_y << 5);
		}
	private:
		uint32_t _y = 2463534242;
	};

	struct Xor128 {
		Xor128() {

		}
		Xor128(uint32_t seed) {
			_w = seed;
		}

		uint32_t generate() {
			uint32_t t = _x ^ (_x << 11);
			_x = _y;
			_y = _z;
			_z = _w;
			return _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
		}
	private:
		uint32_t _x = 123456789;
		uint32_t _y = 362436069;
		uint32_t _z = 521288629;
		uint32_t _w = 88675123;
	};

	struct MersenneTwister {
		MersenneTwister() {

		}
		MersenneTwister(uint32_t seed) :_engine(seed) {
		}

		
		uint32_t generate() {
			return _engine();
		}
	private:
		std::mt19937 _engine;
	};

	template <class Generator>
	struct RandomEngine : public Generator {
	public:
		RandomEngine() :Generator() {
		}
		RandomEngine(uint32_t seed) :Generator(seed) {
		}

		void discard(int n) {
			for (int i = 0; i < n; ++i) {
				this->generate();
			}
		}

		// 0 <= x < 1
		double continuous() {
			uint32_t uniform = this->generate();
			constexpr double c = 1.0 / static_cast<double>(0xffffffffLL + 1);
			return static_cast<double>(uniform) * c;
		}

		// a <= x < b
		double continuous(double a, double b) {
			return a + continuous() * (b - a);
		}

		// R = 1
		Vec2 on_circle() {
			double r_sqrt = glm::sqrt(this->continuous());
			double theta = this->continuous() * glm::two_pi<double>();
			double x = r_sqrt * glm::cos(theta);
			double y = r_sqrt * glm::sin(theta);
			return Vec2(x, y);
		}

		// R = 1
		Vec3 on_sphere() {
			double z = this->continuous() * 2.0 - 1.0;
			double phi = this->continuous() * glm::two_pi<double>();
			double v = glm::sqrt(1.0 - z * z);
			return Vec3(
				v * glm::cos(phi),
				v * glm::sin(phi),
				z
			);
		}

		// R = 1
		Vec3 on_hemisphere() {
			double z = this->continuous() * 2.0 - 1.0;
			double phi = this->continuous() * glm::pi<double>();
			double v = glm::sqrt(1.0 - z * z);
			return Vec3(
				v * glm::cos(phi),
				v * glm::sin(phi),
				z
			);
		}
	};

	typedef RandomEngine<Xor> DefaultEngine;

	//// typedef MersenneTwister DefaultEngine;
	//// typedef LCGs DefaultEngine;
	//typedef Xor DefaultEngine;
	//// typedef Xor128 DefaultEngine;
}
