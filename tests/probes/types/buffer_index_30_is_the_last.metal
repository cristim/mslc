// EXPECT: valid
//
// Apple: buffer indices are 0 to 30.
#include <metal_stdlib>
using namespace metal;
kernel void buffer_index_30_is_the_last(device uint* out [[buffer(30)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
