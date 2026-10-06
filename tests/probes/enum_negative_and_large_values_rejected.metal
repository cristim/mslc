// EXPECT: error needs a 64-bit type
//
// -1 and 0xffffffff share no 32-bit type; Apple widens the enum to long, mslc has none for an enum.
#include <metal_stdlib>
using namespace metal;
enum { A = 0xffffffff, B = -1 };
kernel void enum_negative_and_large_values_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
