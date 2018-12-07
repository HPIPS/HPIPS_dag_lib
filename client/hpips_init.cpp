#include "init.h"

#ifdef __cplusplus         // if used by C++ code
extern "C" {                  // we need to export the C interface
#endif

	__declspec(dllexport) int addfun(int a, int b)
	{
		return a + b;
	}

#ifdef __cplusplus
}
#endif