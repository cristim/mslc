// EXPECT: valid
// REFLECT: "metal_index": 1
// The condition of a conditional in an attribute index is a bool, which is fine there: the index is an int.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(false ? 0 : 1)]], uint i [[thread_position_in_grid]]) { out[i] = 1.0f; }
