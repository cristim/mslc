// EXPECT: error operator + on a bool vector is not lowered yet
// Only the bitwise operators are lowered on a bool vector.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { bool2 v = bool2(i > 1, i > 2); bool2 w = v + v; out[i] = float(w.x); }
