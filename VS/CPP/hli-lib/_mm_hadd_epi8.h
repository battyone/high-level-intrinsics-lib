#pragma once


//#include "mmintrin.h"  // mmx
#include "emmintrin.h"  // sse
#include "pmmintrin.h"  // sse3
#include "tmmintrin.h"  // ssse3
#include "smmintrin.h"  // sse4.1
#include "nmmintrin.h"  // sse4.2
#include "immintrin.h"  // avx, avx2, avx512, FP16C, KNCNI, FMA
//#include "ammintrin.h"  // AMD-specific intrinsics

#include <math.h>       /* sqrt */

namespace hli {


	inline __m128i _mm_hadd_epi8(
		const __m128i * const mem_addr,
		const size_t nBytes)
	{
		return _mm_setzero_si128();
	}

	inline __m128i _mm_hadd_epi8_ref(
		const __m128i * const mem_addr,
		const size_t nBytes)
	{
		const __int8 * const ptr = reinterpret_cast<const __int8 * const>(mem_addr);
		__int64 sum = 0;
		for (size_t i = 0; i < nBytes; ++i) {
			sum += ptr[i];
			//std::cout << "INFO: hli::ref::_mm_hadd_epi8: i=" << i << "; sum="<<sum << std::endl;
		}
		return _mm_set1_epi64x(sum);
	}

}