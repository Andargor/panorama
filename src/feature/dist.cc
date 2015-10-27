//File: dist.cc
//Date:
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#include "dist.hh"
#include "lib/debugutils.hh"
#include "lib/common.hh"

#include <limits>

#ifdef __SSE3__
#include <x86intrin.h>

namespace feature {

// %35 faster
float euclidean_sqr(
		const float* x, const float* y,
		int n, float now_thres) {
	float ans = 0;
	__m128 vsum = _mm_set1_ps(0.0f);
	m_assert(n % 4 == 0);
	for (; n > 0; n -= 4) {
		const __m128 a = _mm_loadu_ps(x);
		const __m128 b = _mm_loadu_ps(y);
		const __m128 diff = _mm_sub_ps(a, b);
		const __m128 sqr = _mm_mul_ps(diff, diff);
		vsum = _mm_add_ps(vsum, sqr);

		// check result temporarily. slightly faster
		if (n % 32 == 0) {
			__m128 rst = _mm_hadd_ps(vsum, vsum);
			rst = _mm_hadd_ps(rst, rst);
			_mm_store_ss(&ans, rst);
			if (ans > now_thres)
				return std::numeric_limits<float>::max();
		}

		x += 4;
		y += 4;
		_mm_prefetch(x, _MM_HINT_T0);
		_mm_prefetch(y, _MM_HINT_T0);
	}
	__m128 rst = _mm_hadd_ps(vsum, vsum);
	rst = _mm_hadd_ps(rst, rst);
	_mm_store_ss(&ans, rst);
	return ans;
}

#else

float euclidean_sqr(
		const float* x, const float* y,
		int size, float now_thres) {
	float ans = 0;
	REP(i, size) {
		ans += sqr(x[i] - y[i]);
		if (ans > now_thres)
			return std::numeric_limits<float>::max();
	}
	return ans;
}

#endif

int hamming(const float* x, const float* y, int n) {
	int sum = 0;
	REP(i, n) {
		unsigned int* p1 = (unsigned int*)&x[i];
		unsigned int* p2 = (unsigned int*)&y[i];
		sum += __builtin_popcount((*p1) ^ *(p2));
	}
	return sum;
}

}
