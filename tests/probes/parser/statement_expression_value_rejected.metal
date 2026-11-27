// EXPECT: error a statement expression "({ ... })" is lowered only as a statement of its own
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = ({ uint t = i; t + 1u; }); }
