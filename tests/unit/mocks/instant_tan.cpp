// instantTan() for the filter tests: util/functions.cpp, which has the real one, pulls in too much.
// Keep this the same as the real one.

#include "util/fixedpoint.h"
#include "util/lookuptables/lookuptables.h"
#include <cstdint>

int32_t instantTan(int32_t input) {
	int32_t whichValue = input >> 25;                   // 25
	int32_t howMuchFurther = (input << 6) & 2147483647; // 6
	int32_t value1 = tanTable[whichValue];
	int32_t value2 = tanTable[whichValue + 1];
	return (multiply_32x32_rshift32(value2, howMuchFurther)
	        + multiply_32x32_rshift32(value1, 2147483647 - howMuchFurther))
	       << 1;
}
