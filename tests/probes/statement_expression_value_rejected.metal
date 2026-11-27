// EXPECT: error a statement expression "({ ... })" whose value is used is not lowered
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = ({ uint t = i; t + 1u; }); }
