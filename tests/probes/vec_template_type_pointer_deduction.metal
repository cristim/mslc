// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// thread vec<T, 4>* deduces T from a local array of four-component vectors.
#include <metal_stdlib>
using namespace metal;
template <typename T> static T f(thread vec<T, 4>* p) { return p[0].x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float4 a[2] = {float4(1), float4(2)}; out[i] = f(a) + f<float>(a); }
