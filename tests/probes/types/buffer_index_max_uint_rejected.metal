// EXPECT: error a long is not an index
//
// 4294967295 is a long, which is what Apple's "invalid type 'long' for attribute index
// expression" says too.
#include <metal_stdlib>
using namespace metal;
kernel void buffer_index_max_uint_rejected(device uint* out [[buffer(4294967295)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
