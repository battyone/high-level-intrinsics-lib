#pragma once

#include <algorithm>	// std::min
#include <limits>		// std::numeric_limits
#include <iostream>		// std::cout
#include <tuple>
#include <array>
#include <map>

//#include "mmintrin.h"  // mmx
#include "emmintrin.h"  // sse
#include "pmmintrin.h"  // sse3
#include "tmmintrin.h"  // ssse3
#include "smmintrin.h"  // sse4.1
#include "nmmintrin.h"  // sse4.2
#include "immintrin.h"  // for _mm_log2_pd
//#include "immintrin.h"  // avx, avx2, avx512, FP16C, KNCNI, FMA
//#include "ammintrin.h"  // AMD-specific intrinsics

#include "intrin.h"

#include "_mm_rand_si128.ipp"
#include "tools.ipp"
#include "timer.ipp"

namespace hli {

	namespace priv {

#		ifdef _MSC_VER
		inline __m128d _mm_log2_pd(__m128d a) {
			return _mm_set_pd(log2(a.m128d_f64[1]), log2(a.m128d_f64[0]));
		}
#		endif

		template <int N_BITS1, int N_BITS2, bool MIS_VALUE>
		inline void merge(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const std::tuple<__m128i * const, const size_t>& data3)
		{
			static_assert(N_BITS1 > 0, "NBITS_1 has to be larger than zero");
			static_assert(N_BITS2 > 0, "NBITS_2 has to be larger than zero");
			static_assert((N_BITS1 + N_BITS2) <= 8, "NBITS_1 + N_BITS_2 has to be smaller than 9");

			const size_t nBlocks = std::get<1>(data1) >> 4;
			for (size_t block = 0; block < nBlocks; ++block) {
				std::get<0>(data3)[block] = _mm_or_si128(std::get<0>(data1)[block], _mm_slli_epi16(std::get<0>(data2)[block], N_BITS1));
			}
		}

		inline __m128d freq_2bits_to_entropy_ref(
			const __m128i freq)
		{
			const __m128i nValues0 = _mm_shuffle_epi32(freq, 0b00000000);
			const __m128i nValues1 = _mm_shuffle_epi32(freq, 0b01010101);
			const __m128i nValues2 = _mm_shuffle_epi32(freq, 0b10101010);
			const __m128i nValues = _mm_add_epi32(nValues0, _mm_add_epi32(nValues1, nValues2));
			const __m128d nValuesDP = _mm_cvtepi32_pd(nValues);

			const __m128d prob1 = _mm_div_pd(_mm_cvtepi32_pd(freq), nValuesDP);
			const __m128d prob2 = _mm_div_pd(_mm_cvtepi32_pd(_mm_swap_64(freq)), nValuesDP);

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

				const int nBits0 = static_cast<int>(_mm_popcnt_u64(
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask0))) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d1, mask0)) << 2) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d2, mask0)) << 4) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d3, mask0)) << 5)));

				const int nBits1 = static_cast<int>(_mm_popcnt_u64(
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask1))) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d1, mask1)) << 2) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d2, mask1)) << 4) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d3, mask1)) << 5)));

				const int nBits2 = static_cast<int>(_mm_popcnt_u64(
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask2))) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d1, mask2)) << 2) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d2, mask2)) << 4) ||
					(_mm_movemask_epi8(_mm_cmpeq_epi8(d3, mask2)) << 6)));

				freq = _mm_add_epi32(freq, _mm_set_epi32(nBits0, nBits1, nBits2, 0));
			}

			for (; block < nBlocks; ++block) {
				const __m128i d0 = ptr[block];
				const int nBits0 = static_cast<int>(_mm_popcnt_u64(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask0))));
				const int nBits1 = static_cast<int>(_mm_popcnt_u64(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask1))));
				const int nBits2 = static_cast<int>(_mm_popcnt_u64(_mm_movemask_epi8(_mm_cmpeq_epi8(d0, mask2))));
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
					(_mm_movemask_epi8(_mm_slli_si128(d1, 6)) << 2) ||
					(_mm_movemask_epi8(_mm_slli_si128(d2, 6)) << 4) ||
					(_mm_movemask_epi8(_mm_slli_si128(d3, 6)) << 6));
				
				const __int64 bit1 =
					(_mm_movemask_epi8(_mm_slli_si128(d0, 7)) ||
					(_mm_movemask_epi8(_mm_slli_si128(d1, 7)) << 2) ||
					(_mm_movemask_epi8(_mm_slli_si128(d2, 7)) << 4) ||
					(_mm_movemask_epi8(_mm_slli_si128(d3, 7)) << 6));
				
				/*
				const __m256i de0, de1;
				const __int64 bit0e =
					(_mm256_movemask_epi8(_mm256_slli_si256(de0, 7)) ||
					(_mm256_movemask_epi8(_mm256_slli_si256(de1, 7)) << (1 * 32)));
				*/



			}
			return _mm_setzero_si128();
		}


		template <int N_BITS, bool MIS_VALUE>
		inline __m128d _mm_entropy_epu8_method0(
			const std::tuple<const __m128i * const, const size_t>& data,
			const size_t nElements)
		{
			const __int8 * const ptr1 = reinterpret_cast<const __int8 * const>(std::get<0>(data));

			static_assert(N_BITS > 0, "_mm_entropy_epu8_method0: N_BITS should be larger than 0");
			static_assert(N_BITS <= 8, "_mm_entropy_epu8_method0: N_BITS should be smaller or equal than 8");

			const int N_DISTINCT_VALUES = (1 << N_BITS);
			std::array<size_t, N_DISTINCT_VALUES> freq;
			freq.fill(0);

			for (size_t element = 0; element < nElements; ++element) {
				const __int8 mergedData = ptr1[element];
				freq[mergedData]++;
			}

			// ASSUME THAT NO MISSING VALUES EXIST
			double h = 0;
			for (int i = 0; i < N_DISTINCT_VALUES; ++i)
			{
				//std::cout << "INFO: _mm_entropy_epu8_method0: freq[" << i <<"]=" << freq[i] << std::endl;
				if (freq[i] > 0) {
					double prob = (static_cast<double>(freq[i]) / nElements);
					h += prob * log2(prob);
				}
			}
			return _mm_set1_pd(-h);
		}

		template <int N_BITS, bool MIS_VALUE>
		inline __m128d _mm_entropy_epu8_method1(
			const std::tuple<const __m128i * const, const size_t>& data,
			const size_t nElements)
		{
			static_assert(N_BITS > 0, "");
			static_assert(N_BITS <= 8, "");

			const int N_DISTINCT_VALUES = (1 << N_BITS);
			std::array<size_t, N_DISTINCT_VALUES> freq;
			freq.fill(0);

			const size_t nBlocks = std::get<1>(data) >> 4;

			for (size_t block = 0; block < nBlocks; ++block) {
				const __m128i d = std::get<0>(data)[block];
				for (int i = 0; i < N_DISTINCT_VALUES; ++i) {
					freq[i] += _mm_popcnt_u64(_mm_movemask_epi8(_mm_cmpeq_epi8(d, _mm_set1_epi8(static_cast<char>(i)))));
				}
			}

			//TODO: ERROR here: some training data is not counted

			// ASSUME THAT NO MISSING VALUES EXIST
			double h = 0;
			for (int i = 0; i < N_DISTINCT_VALUES; ++i)
			{
				//std::cout << "INFO: _mm_entropy_epu8_method1: freq[" << i <<"]=" << freq[i] << std::endl;
				if (freq[i] > 0) {
					double prob = (static_cast<double>(freq[i]) / nElements);
					h += prob * log2(prob);
				}
			}
			return _mm_set1_pd(-h);
		}

		template <int N_BITS1, int N_BITS2, bool MIS_VALUE>
		inline __m128d _mm_entropy_epu8_method0(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements)
		{
			static_assert(N_BITS1 > 0, "NBITS_1 has to be larger than zero");
			static_assert(N_BITS2 > 0, "NBITS_2 has to be larger than zero");
			const int N_BITS = N_BITS1 + N_BITS2;
			static_assert(N_BITS <= 8, "NBITS_1 + N_BITS_2 has to be smaller or equal than 8");

			const __int8 * const ptr1 = reinterpret_cast<const __int8 * const>(std::get<0>(data1));
			const __int8 * const ptr2 = reinterpret_cast<const __int8 * const>(std::get<0>(data2));

			const int N_DISTINCT_VALUES = (1 << N_BITS);
			std::array<size_t, N_DISTINCT_VALUES> freq;
			freq.fill(0);

			for (size_t element = 0; element < nElements; ++element) {
				const __int8 mergedData = ptr1[element] | (ptr2[element] << N_BITS1);
				freq[mergedData]++;
			}

			// ASSUME THAT NO MISSING VALUES EXIST
			double h = 0;
			for (int i = 0; i < N_DISTINCT_VALUES; ++i)
			{
				//std::cout << "INFO: _mm_entropy_epu8_method1: freq[" << i <<"]=" << freq[i] << std::endl;
				if (freq[i] > 0) {
					double prob = (static_cast<double>(freq[i]) / nElements);
					h += prob * log2(prob);
				}
			}
			return _mm_set1_pd(-h);
		}

		template <int N_BITS1, int N_BITS2, bool MIS_VALUE>
		inline __m128d _mm_entropy_epu8_method1(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements)
		{
			const std::tuple<__m128i * const, const size_t> data3 = _mm_malloc_m128i(std::get<1>(data1));
			merge<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, data3);
			const int N_BITS3 = N_BITS1 + N_BITS2;
			const __m128d result = _mm_entropy_epu8<N_BITS3, MIS_VALUE>(data3, nElements);
			_mm_free2(data3);
			return result;
		}

		template <int N_BITS1, int N_BITS2, bool MIS_VALUE>
		inline __m128d _mm_entropy_epu8_method2(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements)
		{
			//TODO
			return _mm_entropy_epu8_method0<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
		}
		
		template <int N_BITS1, int N_BITS2, bool MIS_VALUE>
		inline __m128d _mm_entropy_epu8_method3(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements)
		{
			//TODO
			return _mm_entropy_epu8_method0<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
		}
	}

	namespace test
	{
		void test_mm_entropy_epu8(const size_t nBlocks, const size_t nExperiments, const bool doTests)
		{
			const double delta = 0.0000001;
			const bool MIS_VALUE = false;
			const size_t nElements = 16 * nBlocks;
			const int N_BITS1 = 3;
			const int N_BITS2 = 2;

			auto data1 = _mm_malloc_m128i(nElements);
			auto data2 = _mm_malloc_m128i(nElements);
			fillRand_epu8<N_BITS1>(data1);
			fillRand_epu8<N_BITS2>(data2);

			double min0 = std::numeric_limits<double>::max();
			double min1 = std::numeric_limits<double>::max();
			double min2 = std::numeric_limits<double>::max();
			double min3 = std::numeric_limits<double>::max();
			double min4 = std::numeric_limits<double>::max();

			__m128d result0, result1, result2, result3;

			for (size_t i = 0; i < nExperiments; ++i) {

				timer::reset_and_start_timer();
				result0 = hli::priv::_mm_entropy_epu8_method0<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
				min0= std::min(min0, timer::get_elapsed_kcycles());

				{
					timer::reset_and_start_timer();
					result1 = hli::priv::_mm_entropy_epu8_method1<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
					min1 = std::min(min1, timer::get_elapsed_kcycles());

					if (doTests) {
						if (std::abs(result0.m128d_f64[0] - result1.m128d_f64[0]) > delta) {
							std::cout << "WARNING: test _mm_entropy_epu8_method0<" << N_BITS1 << "," << N_BITS2 << ">: result-ref=" << hli::toString_f64(result0) << "; result=" << hli::toString_f64(result1) << std::endl;
							return;
						}
					}
				}
				{
					timer::reset_and_start_timer();
					result2 = hli::priv::_mm_entropy_epu8_method2<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
					min2 = std::min(min2, timer::get_elapsed_kcycles());

					if (doTests) {
						if (std::abs(result0.m128d_f64[0] - result2.m128d_f64[0]) > delta) {
							std::cout << "WARNING: test _mm_entropy_epu8_method1<" << N_BITS1 << "," << N_BITS2 << ">: result-ref=" << hli::toString_f64(result0) << "; result=" << hli::toString_f64(result2) << std::endl;
							return;
						}
					}
				}
				{
					timer::reset_and_start_timer();
					result3 = hli::priv::_mm_entropy_epu8_method3<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
					min3 = std::min(min3, timer::get_elapsed_kcycles());

					if (doTests) {
						if (std::abs(result0.m128d_f64[0] - result3.m128d_f64[0]) > delta) {
							std::cout << "WARNING: test _mm_entropy_epu8_method1<" << N_BITS1 << "," << N_BITS2 << ">: result-ref=" << hli::toString_f64(result0) << "; result=" << hli::toString_f64(result3) << std::endl;
							return;
						}
					}
				}

			}
			printf("[_mm_entropy_epu8_method0<%i,%i>]: %2.5f Kcycles; %0.14f\n", N_BITS1, N_BITS2, min0, result0.m128d_f64[0]);
			printf("[_mm_entropy_epu8_method1<%i,%i>]: %2.5f Kcycles; %0.14f; %2.3f times faster than ref\n", N_BITS1, N_BITS2, min1, result1.m128d_f64[0], min0 / min1);
			printf("[_mm_entropy_epu8_method2<%i,%i>]: %2.5f Kcycles; %0.14f; %2.3f times faster than ref\n", N_BITS1, N_BITS2, min2, result2.m128d_f64[0], min0 / min2);
			printf("[_mm_entropy_epu8_method3<%i,%i>]: %2.5f Kcycles; %0.14f; %2.3f times faster than ref\n", N_BITS1, N_BITS2, min3, result3.m128d_f64[0], min0 / min3);
		
			_mm_free2(data1);
			_mm_free2(data2);
		}
	}
	
	template <int N_BITS, bool MIS_VALUE>
	inline __m128d _mm_entropy_epu8(
		const std::tuple<const __m128i * const, const size_t>& data,
		const size_t nElements)
	{
		const __m128d result = priv::_mm_entropy_epu8_method0<N_BITS, MIS_VALUE>(data, nElements);
#		if	_DEBUG
		if (isnan(result.m128d_f64[0])) std::cout << "WARNING: _mm_entropy_epu8: result is NAN" << std::endl;
		if (result.m128d_f64[0] < 0)    std::cout << "WARNING: _mm_entropy_epu8: result is smaller than 0. result=" << result.m128d_f64[0] << std::endl;
		if (result.m128d_f64[0] > N_BITS) std::cout << "WARNING: _mm_entropy_epu8: result is larger than N_BITS=" << N_BITS << ". result=" << result.m128d_f64[0] << std::endl;
#		endif
		return result;
	}

	template <bool MIS_VALUE>
	inline __m128d _mm_entropy_epu8(
		const std::tuple<const __m128i * const, const size_t>& data,
		const int nBits,
		const size_t nElements)
	{
		switch (nBits) {
		case 1: return _mm_entropy_epu8<1, MIS_VALUE>(data, nElements);
		case 2: return _mm_entropy_epu8<2, MIS_VALUE>(data, nElements);
		case 3: return _mm_entropy_epu8<3, MIS_VALUE>(data, nElements);
		case 4: return _mm_entropy_epu8<4, MIS_VALUE>(data, nElements);
		case 5: return _mm_entropy_epu8<5, MIS_VALUE>(data, nElements);
		case 6: return _mm_entropy_epu8<6, MIS_VALUE>(data, nElements);
		case 7: return _mm_entropy_epu8<7, MIS_VALUE>(data, nElements);
		case 8: return _mm_entropy_epu8<8, MIS_VALUE>(data, nElements);
		default: return _mm_setzero_pd();
		}
	}

	template <int N_BITS1, int N_BITS2, bool MIS_VALUE>
	inline __m128d _mm_entropy_epu8(
		const std::tuple<const __m128i * const, const size_t>& data1,
		const std::tuple<const __m128i * const, const size_t>& data2,
		const size_t nElements)
	{
		const __m128d result = priv::_mm_entropy_epu8_method0<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);
		//const __m128d result = priv::_mm_entropy_epu8_method1<N_BITS1, N_BITS2, MIS_VALUE>(data1, data2, nElements);

#		if	_DEBUG
		if (isnan(result.m128d_f64[0])) std::cout << "WARNING: _mm_entropy_epu8: result is NAN" << std::endl;
		if (result.m128d_f64[0] < 0) std::cout << "WARNING: _mm_entropy_epu8: result is smaller than 0. " << result.m128d_f64[0] << std::endl;
#		endif
		return result;
	}

	template <bool MIS_VALUE>
	inline __m128d _mm_entropy_epu8(
		const std::tuple<const __m128i * const, const size_t>& data1,
		const int nBits1,
		const std::tuple<const __m128i * const, const size_t>& data2,
		const int nBits2,
		const size_t nElements)
	{
		switch (nBits1) {
		case 1:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<1, 1, MIS_VALUE>(data1, data2, nElements);
			case 2: return _mm_entropy_epu8<1, 2, MIS_VALUE>(data1, data2, nElements);
			case 3: return _mm_entropy_epu8<1, 3, MIS_VALUE>(data1, data2, nElements);
			case 4: return _mm_entropy_epu8<1, 4, MIS_VALUE>(data1, data2, nElements);
			case 5: return _mm_entropy_epu8<1, 5, MIS_VALUE>(data1, data2, nElements);
			case 6: return _mm_entropy_epu8<1, 6, MIS_VALUE>(data1, data2, nElements);
			case 7: return _mm_entropy_epu8<1, 7, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		case 2:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<2, 1, MIS_VALUE>(data1, data2, nElements);
			case 2: return _mm_entropy_epu8<2, 2, MIS_VALUE>(data1, data2, nElements);
			case 3: return _mm_entropy_epu8<2, 3, MIS_VALUE>(data1, data2, nElements);
			case 4: return _mm_entropy_epu8<2, 4, MIS_VALUE>(data1, data2, nElements);
			case 5: return _mm_entropy_epu8<2, 5, MIS_VALUE>(data1, data2, nElements);
			case 6: return _mm_entropy_epu8<2, 6, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		case 3:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<3, 1, MIS_VALUE>(data1, data2, nElements);
			case 2: return _mm_entropy_epu8<3, 2, MIS_VALUE>(data1, data2, nElements);
			case 3: return _mm_entropy_epu8<3, 3, MIS_VALUE>(data1, data2, nElements);
			case 4: return _mm_entropy_epu8<3, 4, MIS_VALUE>(data1, data2, nElements);
			case 5: return _mm_entropy_epu8<3, 5, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		case 4:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<4, 1, MIS_VALUE>(data1, data2, nElements);
			case 2: return _mm_entropy_epu8<4, 2, MIS_VALUE>(data1, data2, nElements);
			case 3: return _mm_entropy_epu8<4, 3, MIS_VALUE>(data1, data2, nElements);
			case 4: return _mm_entropy_epu8<4, 4, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		case 5:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<5, 1, MIS_VALUE>(data1, data2, nElements);
			case 2: return _mm_entropy_epu8<5, 2, MIS_VALUE>(data1, data2, nElements);
			case 3: return _mm_entropy_epu8<5, 3, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		case 6:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<6, 1, MIS_VALUE>(data1, data2, nElements);
			case 2: return _mm_entropy_epu8<6, 2, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		case 7:
			switch (nBits2)
			{
			case 1: return _mm_entropy_epu8<7, 1, MIS_VALUE>(data1, data2, nElements);
			default: return _mm_setzero_pd();
			}
		default: return _mm_setzero_pd();
		}
	}
}