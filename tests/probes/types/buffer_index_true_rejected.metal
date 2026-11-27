// EXPECT: error needs a constant integer argument
//
// Apple: "invalid subexpression in attribute index expression".
#include <metal_stdlib>
using namespace metal;
kernel void buffer_index_true_rejected(device uint* out [[buffer(true)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
