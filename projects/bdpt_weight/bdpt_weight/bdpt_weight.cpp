// bdpt_weight.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include <iostream>

int main()
{
	for (int s = 0; s < 4; ++s) {
		for (int t = 0; t < 4; ++t) {
			double w = 1.0 / (t + s + 1.0);
			printf("%f\n", w);
		}
	}
	std::cin.get();
    return 0;
}

