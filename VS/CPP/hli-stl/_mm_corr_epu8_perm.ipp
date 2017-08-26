#pragma once

#include <algorithm>	// std::min
#include <limits>		// std::numeric_limits
#include <iostream>		// std::cout
#include <tuple>

//#include "mmintrin.h"  // mmx
#include "emmintrin.h"  // sse
#include "pmmintrin.h"  // sse3
#include "tmmintrin.h"  // ssse3
#include "smmintrin.h"  // sse4.1
#include "nmmintrin.h"  // sse4.2
//#include "immintrin.h"  // avx, avx2, avx512, FP16C, KNCNI, FMA
//#include "ammintrin.h"  // AMD-specific intrinsics

#include "tools.ipp"
#include "timer.ipp"

#include "_mm_rand_si128.ipp"


namespace hli {

	namespace priv {

		// Uses Reference implementation
		template <bool HAS_MV, U8 MV>
		inline void _mm_corr_epu8_perm_method0(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements,
			const std::tuple<__m128d * const, const size_t>& results,
			const size_t nPermutations,
			__m128i& randInts)
		{
			auto data3 = deepCopy(data2);
			auto swap = _mm_malloc_m128i(nElements << 1);

			double * const results_double = reinterpret_cast<double * const>(std::get<0>(results));
			for (size_t permutation = 0; permutation < nPermutations; ++permutation)
			{
				_mm_permute_epu8_array_method0(data3, nElements, swap, randInts);
				const __m128d corr1 = _mm_corr_epu8_ref<HAS_MV, MV>(data1, data3, nElements);
				results_double[permutation] = corr1.m128d_f64[0];
			}
			_mm_free2(data3);
			_mm_free2(swap);
		}

		template <int N_BITS, bool HAS_MV, U8 MV>
		inline void _mm_corr_epu8_perm_method1(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements,
			const std::tuple<__m128d * const, const size_t>& results,
			const size_t nPermutations,
			__m128i& randInts)
		{
			auto data3 = deepCopy(data2);
			const size_t nBytes = std::get<1>(data1);
			const size_t swap_array_nBytes = nBytes << 1;
			auto swap = _mm_malloc_m128i(swap_array_nBytes);

			const auto tup1 = _mm_hadd_epu8<N_BITS, HAS_MV, MV>(data1, nElements);
			const auto tup2 = _mm_hadd_epu8<N_BITS, HAS_MV, MV>(data2, nElements);
			const __m128d average1 = _mm_div_pd(_mm_cvtepi32_pd(std::get<0>(tup1)), _mm_cvtepi32_pd(std::get<1>(tup1)));
			const __m128d average2 = _mm_div_pd(_mm_cvtepi32_pd(std::get<0>(tup2)), _mm_cvtepi32_pd(std::get<1>(tup2)));

			double * const results_double = reinterpret_cast<double * const>(std::get<0>(results));
			for (size_t permutation = 0; permutation < nPermutations; ++permutation) {
				_mm_permute_epu8_array(data3, nElements, swap, randInts);
				const __m128d corr = _mm_corr_epu8_method1<N_BITS, HAS_MV, MV>(data1, data3, nElements, average1, average2);
				results_double[permutation] = corr.m128d_f64[0];
			}
			_mm_free2(data3);
			_mm_free2(swap);
		}

		template <int N_BITS, bool HAS_MV, U8 MV>
		inline void _mm_corr_epu8_perm_method2(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements,
			const std::tuple<__m128d * const, const size_t>& results,
			const size_t nPermutations,
			__m128i& randInts)
		{
			const size_t nBytes = std::get<1>(data1);
			auto data1_Double = _mm_malloc_m128d(8 * nBytes);
			auto data2_Double = _mm_malloc_m128d(8 * nBytes);
			auto swap = _mm_malloc_m128i(2 * nBytes);

			const __m128d var1 = calc_variance<N_BITS, HAS_MV, MV>(data1, nElements, data1_Double);
			const __m128d var2 = calc_variance<N_BITS, HAS_MV, MV>(data2, nElements, data2_Double);
			const __m128d var1_2 = _mm_sqrt_pd(_mm_mul_pd(var1, var2));

			//std::cout << "INFO: _mm_corr_epu8::_mm_corr_perm_epu8_method3: var1=" << var1.m128d_f64[0] << "; var2=" << var2.m128d_f64[0] << std::endl;

			double * const results_double = reinterpret_cast<double * const>(std::get<0>(results));
			for (size_t permutation = 0; permutation < nPermutations; ++permutation) {
				_mm_permute_pd_array(data2_Double, nElements, swap, randInts);
				const __m128d corr = _mm_corr_dp_method3<HAS_MV, MV>(data1_Double, data2_Double, nElements, var1_2);
				//std::cout << "INFO: _mm_corr_epu8::_mm_corr_perm_epu8_method3: corr=" << corr.m128d_f64[0] << std::endl;
				results_double[permutation] = corr.m128d_f64[0];
			}
			_mm_free2(data1_Double);
			_mm_free2(data2_Double);
			_mm_free2(swap);
		}

		template <bool HAS_MV, U8 MV>
		inline void _mm_corr_epu8_perm_method3(
			const std::tuple<const __m128i * const, const size_t>& data1,
			const std::tuple<const __m128i * const, const size_t>& data2,
			const size_t nElements,
			const std::tuple<__m128d * const, const size_t>& results,
			const size_t nPermutations,
			__m128i& randInts)
		{
			const size_t nBytes = std::get<1>(data1);
			const size_t nBlocks = nBytes >> 4;

			if (nElements > 0xFFFF) {
				std::cout << "WARNING: _mm_corr_epu8_perm: _mm_corr_epu8_perm_method3: nElements=" << nElements << " which is larger than 0xFFFF." << std::endl;
			}

			const __int8 * const ptr1 = reinterpret_cast<const __int8 * const>(std::get<0>(data1));
			const __int8 * const ptr2 = reinterpret_cast<const __int8 * const>(std::get<0>(data2));

			__int32 s11 = 0;
			__int32 s22 = 0;
			__int32 s1 = 0;
			__int32 s2 = 0;

			for (size_t i = 0; i < nElements; ++i)
			{
				const unsigned __int8 d1 = ptr1[i];
				const unsigned __int8 d2 = ptr2[i];
				s11 += d1 * d1;
				s22 += d2 * d2;
				s1 += d1;
				s2 += d2;
			}

			const double s11d = static_cast<double>(s11);
			const double s22d = static_cast<double>(s22);
			const double s1d = static_cast<double>(s1);
			const double s2d = static_cast<double>(s2);

			const size_t swap_array_nBytes = nBytes << 1;
			auto swap = _mm_malloc_m128i(swap_array_nBytes);
			auto data3 = deepCopy(data2);

			const __int8 * const ptr3 = reinterpret_cast<const __int8 * const>(std::get<0>(data3));
			double * const results_Double = reinterpret_cast<double * const>(std::get<0>(results));

			for (size_t permutation = 0; permutation < nPermutations; ++permutation)
			{
				_mm_permute_epu8_array(data3, nElements, swap, randInts);

				__int32 s12 = 0;
				for (size_t i = 0; i < nElements; ++i)
				{
					const unsigned __int8 d1 = ptr1[i];
					const unsigned __int8 d2 = ptr3[i];
					s12 += d1 * d2;
				}
				const double s12d = static_cast<double>(s12);
				double corr = ((nElements * s12d) - (s1d*s2d)) / (sqrt((nElements*s11d) - (s1d*s1d)) * sqrt((nElements * s22d) - (s2d*s2d)));
				results_Double[permutation] = corr;
			}
			_mm_free2(data3);
			_mm_free2(swap);
		}
	}

	namespace test {

		void test_mm_corr_epu8_perm(
			const size_t nBlocks,
			const size_t nPermutations,
			const size_t nExperiments,
			const bool doTests)
		{
			const double delta = 0.000001;
			const bool HAS_MV = false;
			const U8 MV = 0xFF;
			const size_t nElements = nBlocks * 16;
			const int N_BITS1 = 5;
			const int N_BITS2 = N_BITS1;


			auto data1_r = _mm_malloc_m128i(nElements);
			auto data2_r = _mm_malloc_m128i(nElements);

			fillRand_epu8<N_BITS1>(data1_r);
			fillRand_epu8<N_BITS2>(data2_r);

			const std::tuple<const __m128i * const, const size_t> data1 = data1_r;
			const std::tuple<const __m128i * const, const size_t> data2 = data2_r;

			const size_t nBytesResults = resizeNBytes(8 * nPermutations, 16);
			//std::cout << "INFO: test_mm_corr_perm_epu8: nPermutations=" << nPermutations << "; nBytesResults=" << nBytesResults << std::endl;
			auto results0 = _mm_malloc_m128d(nBytesResults);
			auto results1 = _mm_malloc_m128d(nBytesResults);
			auto results2 = _mm_malloc_m128d(nBytesResults);
			auto results3 = _mm_malloc_m128d(nBytesResults);

			const __m128i seed = _mm_set_epi16((short)rand(), (short)rand(), (short)rand(), (short)rand(), (short)rand(), (short)rand(), (short)rand(), (short)rand());
			__m128i randInt0 = seed;
			__m128i randInt1 = seed;
			__m128i randInt2 = seed;
			__m128i randInt3 = seed;

			{
				double min0 = std::numeric_limits<double>::max();
				double min1 = std::numeric_limits<double>::max();
				double min2 = std::numeric_limits<double>::max();
				double min3 = std::numeric_limits<double>::max();

				for (size_t i = 0; i < nExperiments; ++i)
				{
					timer::reset_and_start_timer();
					hli::priv::_mm_corr_epu8_perm_method0<HAS_MV, MV>(data1, data2, nElements, results0, nPermutations, randInt0);
					min0 = std::min(min0, timer::get_elapsed_kcycles());

					{
						timer::reset_and_start_timer();
						hli::priv::_mm_corr_epu8_perm_method1<N_BITS1, HAS_MV, MV>(data1, data2, nElements, results1, nPermutations, randInt1);
						min1 = std::min(min1, timer::get_elapsed_kcycles());

						//for (size_t block = 0; block < (nBytesResults >> 4); ++block) {
						//	std::cout << "WARNING: test_mm_corr_perm_epu8_ref<6>: results[" << block << "] =" << hli::toString_f64(results[block]) << std::endl;
						//	std::cout << "WARNING: test_mm_corr_perm_epu8_ref<6>: results1[" << block << "]=" << hli::toString_f64(results1[block]) << std::endl;
						//}

						if (doTests) {
							if (!equal(randInt0, randInt1)) {
								std::cout << "WARNING: test_mm_corr_perm_epu8_ref: randInt=" << hli::toString_u32(randInt0) << "; randInt1=" << hli::toString_u32(randInt1) << std::endl;
								return;
							}
							if (i == 0) {
								for (size_t j = 0; j < (nBytesResults >> 3); ++j) {
									double diff = std::abs(getDouble(results0, j) - getDouble(results1, j));
									if (diff > delta) {
										std::cout << "WARNING: _mm_corr_perm_epu8_method1<6>: j=" << j << "; diff=" << std::setprecision(30) << diff << "; result-ref=" << getDouble(results0, j) << "; result1=" << getDouble(results1, j) << std::endl;
										return;
									}
								}
							}
						}
					}
					{
						timer::reset_and_start_timer();
						hli::priv::_mm_corr_epu8_perm_method2<N_BITS1, HAS_MV, MV>(data1, data2, nElements, results2, nPermutations, randInt2);
						min2 = std::min(min2, timer::get_elapsed_kcycles());

						if (doTests) {
							if (!equal(randInt0, randInt2)) {
								std::cout << "WARNING: _mm_corr_perm_epu8_method2<6>: randInt=" << hli::toString_u32(randInt0) << "; randInt2=" << hli::toString_u32(randInt2) << std::endl;
								return;
							}
							if (i == 0) {
								for (size_t j = 0; j < (nBytesResults >> 3); ++j) {
									double diff = std::abs(getDouble(results0, j) - getDouble(results2, j));
									if (diff > delta) {
										std::cout << "WARNING: _mm_corr_perm_epu8_method3<6>: i=" << j << "; diff=" << std::setprecision(30) << diff << "; result-ref=" << getDouble(results0, j) << "; result2=" << getDouble(results2, j) << std::endl;
										return;
									}
								}
							}
						}
					}
					{
						timer::reset_and_start_timer();
						hli::priv::_mm_corr_epu8_perm_method3<HAS_MV, MV>(data1, data2, nElements, results3, nPermutations, randInt3);
						min3 = std::min(min3, timer::get_elapsed_kcycles());

						if (doTests) {
							if (!equal(randInt0, randInt3)) {
								std::cout << "WARNING: _mm_corr_perm_epu8_method3<6>: randInt=" << hli::toString_u32(randInt0) << "; randInt3=" << hli::toString_u32(randInt3) << std::endl;
								return;
							}
							if (i == 0) {
								for (size_t j = 0; j < (nBytesResults >> 3); ++j) {
									double diff = std::abs(getDouble(results0, j) - getDouble(results3, j));
									if (diff > delta) {
										std::cout << "WARNING: _mm_corr_perm_epu8_method3<6>: i=" << j << "; diff=" << std::setprecision(30) << diff << "; result-ref=" << getDouble(results0, j) << "; result3=" << getDouble(results3, j) << std::endl;
										return;
									}
								}
							}
						}
					}
				}
				printf("[_mm_corr_perm_epu8_method0]: %2.5f Kcycles\n", min0);
				printf("[_mm_corr_perm_epu8_method1]: %2.5f Kcycles; %2.3f times faster than ref\n", min1, min0 / min1);
				printf("[_mm_corr_perm_epu8_method2]: %2.5f Kcycles; %2.3f times faster than ref\n", min2, min0 / min2);
				printf("[_mm_corr_perm_epu8_method3]: %2.5f Kcycles; %2.3f times faster than ref\n", min3, min0 / min3);
			}
			_mm_free2(data1);
			_mm_free2(data2);
			_mm_free2(results0);
			_mm_free2(results1);
			_mm_free2(results2);
			_mm_free2(results3);
		}
	}

	template <int N_BITS, bool HAS_MV, U8 MV>
	inline void _mm_corr_epu8_perm(
		const std::tuple<const __m128i * const, const size_t>& data1,
		const std::tuple<const __m128i * const, const size_t>& data2,
		const size_t nElements,
		const std::tuple<__m128d * const, const size_t>& results,
		const size_t nPermutations,
		__m128i& randInts)
	{
		priv::_mm_corr_epu8_perm_method3<HAS_MV, MV>(data1, data2, nElements, results, nPermutations, randInts);
#		if _DEBUG
		const double * const ptr = reinterpret_cast<double * const>(std::get<0>(results));
		for (size_t i = 0; i < nPermutations; ++i) {
			if ((ptr[i] < -1) || (ptr[i] > 1)) {
				std::cout << "WARNING: _mm_corr_epu8: _mm_corr_perm_epu8: permutation " << i << " has an invalid correlation value " << ptr[i] << std::endl;
			}
		}
#		endif
	}
}