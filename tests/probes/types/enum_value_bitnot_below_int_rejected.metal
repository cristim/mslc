// EXPECT: error does not fit in an int
//
// Apple accepts ~0x7fffffff, which is INT_MIN; mslc holds enumerators to +-INT_MAX.
#include <metal_stdlib>
using namespace metal;
enum One { A = ~2147483647 };
kernel void enum_value_bitnot_below_int_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
