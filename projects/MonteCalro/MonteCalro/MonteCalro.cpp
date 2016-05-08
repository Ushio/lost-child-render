
#include "stdafx.h"

#include "random_engine.hpp"

int main()
{
	using namespace lc;

	Xor128 e;

	int N = 100000;
	double s = 0.0;

	auto f = [](double x, double y) {
		return (x * x + y * y) < 1.0 ? 1.0 : 0.0;
	};

	for (int i = 0; i < N; ++i) {
		double x = generate_continuous(e) * 2.0 - 1.0;
		double y = generate_continuous(e) * 2.0 - 1.0;

		double pdf = 1.0 / 4.0;

		s += f(x, y) / pdf;
	}

	double value = s / N;
	printf("%f\n", value);

	std::cin.get();
    return 0;
}

