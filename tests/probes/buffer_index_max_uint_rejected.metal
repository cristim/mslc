// EXPECT: error index 4294967295 is out of bounds
//
// Apple says "invalid type 'long' for attribute index expression"; both refuse it.
#include <metal_stdlib>
using namespace metal;
kernel void buffer_index_max_uint_rejected(device uint* out [[buffer(4294967295)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
