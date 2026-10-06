// EXPECT: error does not fit in an int or an unsigned int
//
// Apple gives the enum a 64-bit type; mslc has none for an enum.
#include <metal_stdlib>
using namespace metal;
enum { A = 0x100000000 };
kernel void enum_value_beyond_unsigned_int_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
