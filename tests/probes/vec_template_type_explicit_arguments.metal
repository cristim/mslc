// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// vec<T, N> with both template arguments given explicitly.
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> static T first(vec<T, N> v) { return v[0]; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = first<float, 3>(float3(1, 2, 3)) + first<int, 2>(int2(4, 5)); }
