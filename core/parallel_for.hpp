#pragma once

#define PPL_PARALLEL 1
#define TBB_PARALLEL 0

#if PPL_PARALLEL 
#include <ppl.h>
#endif

#if TBB_PARALLEL
#include <tbb/tbb.h>
#endif

template <class F>
inline void parallel_for(int count, F &action /*begin, end*/) {
#if PPL_PARALLEL && TBB_PARALLEL == 0
	concurrency::parallel_for<int>(0, count, [&action](int i) {
		action(i, i + 1);
	}, concurrency::auto_partitioner());

	//int splitN = count;
	//splitN = std::max(splitN, 1);
	//splitN = std::min(splitN, count);

	//int n = count / splitN;
	//int remaind = count % splitN;

	//// no remaind
	//if (remaind == 0) {
	//	concurrency::parallel_for<int>(0, splitN, [n, &action](int i) {
	//		int b = i * n;
	//		action(b, b + n);
	//	}, concurrency::auto_partitioner());
	//}
	//else {
	//	concurrency::parallel_for<int>(0, splitN + 1, [n, count, splitN, &action](int i) {
	//		int b = i * n;
	//		if (i != splitN) {
	//			action(b, b + n);
	//		}
	//		else {
	//			action(b, count);
	//		}
	//	}, concurrency::auto_partitioner());
	//}
#elif TBB_PARALLEL && PPL_PARALLEL == 0
	tbb::parallel_for(tbb::blocked_range<int>(0, count), [&action](const tbb::blocked_range< int >& range) {
		action((int)range.begin(), (int)range.end());
	});
#else
	action(0, count);
#endif
}
