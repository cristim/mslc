// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// vec<T, N> declares a for-loop variable.
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { int s = 0; for (vec<int, 2> p = int2(0); p.x < 3; p.x++) s += p.x; out[i] = s; }
