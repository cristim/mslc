// EXPECT: error overflows an int
//
// Apple widens the result; mslc holds every step of the fold to an int and says so instead of wrapping.
#include <metal_stdlib>
using namespace metal;
enum One { A = 0x7fffffff, B = A + 1 };
kernel void enum_value_overflow_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
