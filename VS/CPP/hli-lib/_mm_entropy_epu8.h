#pragma once

#include <tuple>

//#include "mmintrin.h"  // mmx
#include "emmintrin.h"  // sse
#include "pmmintrin.h"  // sse3
#include "tmmintrin.h"  // ssse3
#include "smmintrin.h"  // sse4.1
#include "nmmintrin.h"  // sse4.2
#include "immintrin.h"  // for _mm_log2_pd
//#include "immintrin.h"  // avx, avx2, avx512, FP16C, KNCNI, FMA
//#include "ammintrin.h"  // AMD-specific intrinsics

#include "tools.h"

namespace hli {

	namespace priv {

		inline __m128d freq_2bits_to_entropy_ref(
			const __m128i freq)
		{
			const __m128i sum_a = _mm_add_epi32(_mm_shuffle_epi32(freq, 0b00000000), _mm_shuffle_epi32(freq, 0b01010101));
			const __m128i nMissingValuesI = _mm_add_epi32(_mm_shuffle_epi32(freq, 0b10101010), sum_a);
			const __m128d nMissingValuesDP = _mm_cvtepi32_pd(nMissingValuesI);

			const __m128d prob1 = _mm_div_pd(_mm_cvtepi32_pd(freq), nMissingValuesDP);
			const __m128d prob2 = _mm_div_pd(_mm_cvtepi32_pd(_mm_swap_64(freq)), nMissingValuesDP);

			const __m128d sum_a = _mm_mul_pd(prob1, _mm_log2_pd(prob1));
			const __m128d sum_b = _mm_mul_pd(prob2, _mm_log2_pd(prob2));

			const __m128d sum_c = _mm_add_pd(sum_a, _mm_blend_pd(sum_b, _mm_setzero_pd(), 1));
			const __m128d sum_d = _mm_hadd_pd(sum_c, sum_c);
			return sum_d;
			/*
			const unsigned int nRowsNoMissingValues = nRows - nMissingValues;
			double sum = 0;
			for (size_t i = 0; i < 2; ++i) {
				const auto f = freq.get(i);
				if (f > 0) {
					const double probability = static_cast<double>(f) / nRowsNoMissingValues;
					sum += (probability * log2(probability));
				}
			}
			return (sum == 0) ? 0.0 : -sum;
			*/
		}

		// 
		template <int N_BITS>
		inline __m128i _mm_freq_epu8(
			const std::tuple<const __m128i * const, const size_t>& data)
		{
			switch (N_BITS) {
				case 2: return _mm_freq_epu8_nBits2(data)
				default: return _mm_setzero_si128();
			}
		}

		inline __m128i _mm_freq_epu8_nBits2_method1(
			const std::tuple<const __m128i * const, const size_t>& data)
		{
			const __m128i * const ptr = std::get<0>(data);

			const __m128i mask0 = _mm_setzero_si128();
			const __m128i mask1 = _mm_set1_epi8(1);
			const __m128i mask2 = _mm_set1_epi8(2);
			
			__m128i freq = _mm_setzero_si128();

			const size_t nBlocks = std::get<1>(data) >> 4;
			size_t block;

			for (block = 0; block < nBlocks - 4; block += 4) {

				const __m128i d0 = ptr[block + 0];
				const __m128i d1 = ptr[block + 1];
				const __m128i d2 = ptr[block + 2];
				const __m128i d3 = ptr[block + 3];

				const int nBits0 = _popcnt64(
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask0))) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d1, mask0)) << (1 * 16)) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d2, mask0)) << (2 * 16)) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d3, mask0)) << (3 * 16)));

				const int nBits1 = _popcnt64(
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask1))) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d1, mask1)) << (1 * 16)) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d2, mask1)) << (2 * 16)) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d3, mask1)) << (3 * 16)));

				const int nBits2 = _popcnt64(
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask2))) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d1, mask2)) << (1 * 16)) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d2, mask2)) << (2 * 16)) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d3, mask2)) << (3 * 16)));

				freq = _mm_add_epi32(freq, _mm_set_epi32(nBits0, nBits1, nBits2, 0));
			}

			for (; block < nBlocks; ++block) {
				const __m128i d0 = ptr[block];
				const int nBits0 = _popcnt64(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask0)));
				const int nBits1 = _popcnt64(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask1)));
				const int nBits2 = _popcnt64(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask2)));
				freq = _mm_add_epi32(freq, _mm_set_epi32(nBits0, nBits1, nBits2, 0));
			}

			return freq;
		}

		inline __m128i _mm_freq_epu8_nBits2_method2(
			const std::tuple<const __m128i * const, const size_t>& data)
		{
			const __m128i * const ptr = std::get<0>(data);
			
			const __m128i mask_0_epu8 = _mm_setzero_si128();
			const __m128i mask_1_epu8 = _mm_set1_epi8(1);
			const __m128i mask_2_epu8 = _mm_set1_epi8(2);

			__m128i freq = _mm_setzero_si128();
			__m128i freq0 = _mm_setzero_si128();
			__m128i freq1 = _mm_setzero_si128();
			__m128i freq2 = _mm_setzero_si128();

			const size_t nBlocks = std::get<1>(data) >> 4;
			const size_t nLoops = nBlocks >> 6; //divide by 2^6=64

			for (size_t block = 0; block < nBlocks; ++block) {
				const __m128i d0 = ptr[block + 0];
				freq0 = _mm_add_epi8(freq0, _mm_and_si128(_mm_cmpeq_epi8(d0, mask_0_epu8), mask_1_epu8));
				freq1 = _mm_add_epi8(freq1, _mm_and_si128(_mm_cmpeq_epi8(d0, mask_1_epu8), mask_1_epu8));
				freq2 = _mm_add_epi8(freq2, _mm_and_si128(_mm_cmpeq_epi8(d0, mask_2_epu8), mask_1_epu8));

				if ((block & 0x80) == 0) {
					freq0 = _mm_sad_epu8(freq0, mask_0_epu8);
					freq = _mm_add_epi32(freq, _mm_shuffle_epi32(freq0, _MM_SHUFFLE_EPI32_INT(2,3,3,3)));
					freq = _mm_add_epi32(freq, _mm_shuffle_epi32(freq0, _MM_SHUFFLE_EPI32_INT(0,3,3,3)));
					freq0 = _mm_setzero_si128();
				}
			}
			return freq;
		}

		inline __m128i _mm_freq_epu8_nBits2_method3(
			const std::tuple<const __m128i * const, const size_t>& data)
		{
			const __m128i * const ptr = std::get<0>(data);
			

			__m128i freq = _mm_setzero_si128();

			const size_t nBlocks = std::get<1>(data) >> 4;
			size_t block;

			for (block = 0; block < nBlocks - 4; block += 4) 
			{
				const __m128i d0 = ptr[block + 0];
				const __m128i d1 = ptr[block + 1];
				const __m128i d2 = ptr[block + 2];
				const __m128i d3 = ptr[block + 3];


				const __int64 bit0 =
					(_mm_movemask_epi8(_mm_slli_si128(d0, 6)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d1, 6)) << (1 * 16)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d2, 6)) << (2 * 16)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d3, 6)) << (3 * 16)));
				
				const __int64 bit1 =
					(_mm_movemask_epi8(_mm_slli_si128(d0, 7)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d1, 7)) << (1 * 16)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d2, 7)) << (2 * 16)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d3, 7)) << (3 * 16)));
				
				/*
				const __m256i de0, de1;
				const __int64 bit0e =
					(_mm256_movemask_epi8(_mm256_slli_si256(de0, 7)) ||
					(_mm256_movemask_epi8(_mm256_slli_si256(de1, 7)) << (1 * 32)));
				*/



			}
		}
	}

	
	template <int N_BITS>
	inline __m128d _mm_entropy_epu8(
		const std::tuple<const __m128i * const, const size_t>& data1)
	{
		return _mm_setzero_pd();
	}

	template <int N_BITS>
	inline __m128d _mm_entropy_epu8(
		const std::tuple<const __m128i * const, const size_t>& data1,
		const std::tuple<const __m128i * const, const size_t>& data2)
	{
		return _mm_setzero_pd();
	}

}