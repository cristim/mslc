// EXPECT: error does not fit in an int
//
// Apple widens the enum to unsigned; mslc holds enumerators to an int.
#include <metal_stdlib>
using namespace metal;
enum One { A = 0x7fffffff, B };
kernel void enum_implicit_value_overflow_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
