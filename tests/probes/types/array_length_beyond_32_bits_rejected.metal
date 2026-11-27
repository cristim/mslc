// EXPECT: error between 0 and 4294967295
//
// An array length is stored in 32 bits.
#include <metal_stdlib>
using namespace metal;
kernel void array_length_beyond_32_bits_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float[4294967296] v; out[i] = i; }
