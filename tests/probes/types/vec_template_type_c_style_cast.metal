// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// (vec<T, N>)x is a C-style cast to the vector.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float4* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { int4 a = int4(1); out[i] = (vec<float, 4>)a; }
