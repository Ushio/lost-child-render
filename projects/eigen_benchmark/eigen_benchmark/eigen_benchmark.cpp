// eigen_benchmark.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"


#include <iostream>
#include <chrono>
#define EIGEN_NO_DEBUG
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

static const int N = 40000000;

void bench_eigen() {
	auto beg = std::chrono::system_clock::now();
	typedef Eigen::Vector3d Vec3;
	typedef Eigen::Matrix3d Mat3;
	for (int i = 0; i < N; ++i) {
		Vec3 o = Vec3(rand(), rand(), rand()).normalized();
		Vec3 d = Vec3(rand(), rand(), rand()).normalized();
		Vec3 v = o + d * 0.5;
		Mat3 m;
		m << 2.0, 2.2, 1.0,
			0.4, 0.3, 1.0,
			0.1, 1.0, 0.1;
		Vec3 x = m * v;
	}
	auto end = std::chrono::system_clock::now();
	double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
	std::cout << "eigen: " << elapsed << std::endl;
}
void bench_glm() {
	auto beg = std::chrono::system_clock::now();
	typedef glm::dvec3 Vec3;
	typedef glm::dmat3 Mat3;
	for (int i = 0; i < N; ++i) {
		Vec3 o = glm::normalize(Vec3(rand(), rand(), rand()));
		Vec3 d = glm::normalize(Vec3(rand(), rand(), rand()));
		Vec3 v = o + d * 0.5;
		Mat3 m = Mat3(2.0, 2.2, 1.0, 0.4, 0.3, 1.0, 0.1, 1.0, 0.1);
		Vec3 x = m * v;
	}
	auto end = std::chrono::system_clock::now();
	double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count();
	std::cout << "glm: " << elapsed << std::endl;
}

int main() {
	srand(0);
	bench_glm();
	srand(0);
	bench_eigen();

	std::cin.get();
}