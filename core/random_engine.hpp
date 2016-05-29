﻿#pragma once

#include <iostream>
#include <algorithm>
#include <random>
#include <inttypes.h>

#include <glm/glm.hpp>
#include <boost/random.hpp>

#include "render_type.hpp"

namespace lc {
	template <class T>
	struct RandomEngine {
		uint32_t operator()() {
			return static_cast<T*>(this)->operator()();
		}
	};
	struct LCGs : public RandomEngine<LCGs> {
		LCGs() {

		}
		LCGs(uint32_t seed):_e(seed){
		}
		uint32_t operator()() {
			boost::uniform_int<int64_t> d(0, std::numeric_limits<uint32_t>::max());
			boost::variate_generator<boost::minstd_rand&, boost::uniform_int<int64_t>> gen(_e, d) ;
			return gen();
		}
		boost::minstd_rand _e;
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

	struct MersenneTwister : public RandomEngine<MersenneTwister> {
		MersenneTwister() {

		}
		MersenneTwister(uint32_t seed):_engine(seed){
		}

		std::mt19937 _engine;
		uint32_t operator()() {
			return _engine();
		}
	};

	// [0, 1)
	template <class E>
	double generate_continuous(RandomEngine<E> &engine) {
		uint32_t uniform = engine();
		constexpr double c = 1.0 / static_cast<double>(0xffffffffLL + 1);
		return static_cast<double>(uniform) * c;
	}
	template <class E>
	double generate_continuous(RandomEngine<E> &engine, double a, double b) {
		return glm::mix(a, b, generate_continuous(engine));
	}

	template <class T>
	struct Sample {
		T value;
		double pdf = 0.0;
	};

	template <class E>
	Vec3 generate_on_sphere(RandomEngine<E> &engine) {
		double z = generate_continuous(engine) * 2.0 - 1.0;
		double phi = generate_continuous(engine) * glm::two_pi<double>();
		double v = glm::sqrt(1.0 - z * z);
		return Vec3(
			v * glm::cos(phi),
			v * glm::sin(phi),
			z);
	}
	template <class E>
	Sample<Vec3> generate_cosine_weight_hemisphere(RandomEngine<E> &engine) {
		double eps = 0.0001;

		double eps_1 = generate_continuous(engine);
		double eps_2 = generate_continuous(engine);

		double theta = glm::acos(glm::sqrt(1.0 - eps_1));
		double phi = glm::two_pi<double>() * eps_2;
		double cos_theta = glm::cos(theta);

		double z = sin(theta) * cos(phi);
		double x = sin(theta) * sin(phi);
		double y = cos_theta;

		Sample<Vec3> sample;
		sample.value = Vec3(x, y, z);
		sample.pdf = glm::max(cos_theta * glm::one_over_pi<double>(), eps);
		return sample;
	}
}
