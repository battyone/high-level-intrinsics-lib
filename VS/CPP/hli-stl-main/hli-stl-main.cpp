#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX 1 
#endif
#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#endif

#include <iostream>		// for cout
#include <stdlib.h>		// for printf
#include <time.h>
#include <algorithm>	// for std::min
#include <limits>		// for numeric_limits

#include <tuple>
#include <vector>
#include <string>
#include <chrono>

#include "..\hli-stl\toString.ipp"
#include "..\hli-stl\timer.ipp"
#include "..\hli-stl\equal.ipp"
#include "..\hli-stl\tools.ipp"

#include "..\hli-stl\_mm_hadd_epu8.ipp"
#include "..\hli-stl\_mm_variance_epu8.ipp"
#include "..\hli-stl\_mm_corr_epu8.ipp"

#include "..\hli-stl\_mm_rand_si128.ipp"
#include "..\hli-stl\_mm_rescale_epu16.ipp"
#include "..\hli-stl\_mm_permute_array.ipp"
#include "..\hli-stl\_mm_entropy_epu8.ipp"
#include "..\hli-stl\_mm_mi_epu8.ipp"

int main()
{
#	ifdef _MSC_VER
#		if _DEBUG
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#		endif
#	endif
	{
		const auto start = std::chrono::system_clock::now();

		const size_t nExperiments = 1000;
		//hli::test_endianess();

		//hli::test::test_mm_hadd_epu8(10010, nExperiments, true);
		//hli::test::test_mm_variance_epu8(10010, nExperiments, true);
		//hli::test::test_mm_corr_epu8(1010, nExperiments, true);
		//hli::test::test_mm_corr_pd(1010, nExperiments, true);


		//hli::test::test_mm_rand_si128(1010, nExperiments, true);
		//hli::test::test_mm_rescale_epu16(2102, nExperiments, true);
		//hli::test::test_mm_permute_epu8_array(2102, nExperiments, true);
		//hli::test::test_mm_permute_dp_array(3102, nExperiments, true);

		//hli::test::test_mm_corr_perm_epu8(110, 1000, nExperiments, true);

		hli::test::test_mm_entropy_epu8(100, nExperiments, true);
		//hli::test::test_mm_mi_epu8(100, nExperiments, true);
		//hli::test::test_mm_mi_perm_epu8(100, 1000, nExperiments, true);

		const auto diff = std::chrono::system_clock::now() - start;
		std::cout << std::endl
			<< "DONE: passed time: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms = "
			<< std::chrono::duration_cast<std::chrono::seconds>(diff).count() << " sec = "
			<< std::chrono::duration_cast<std::chrono::minutes>(diff).count() << " min = "
			<< std::chrono::duration_cast<std::chrono::hours>(diff).count() << " hours" << std::endl;

		printf("\n-------------------\n");
		printf("\nPress RETURN to finish:");
	}

#	ifdef _MSC_VER
#		if _DEBUG
	_CrtDumpMemoryLeaks();
#		endif
#	endif

	getchar();
	return 0;
}
