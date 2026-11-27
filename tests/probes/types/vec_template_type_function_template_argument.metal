// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// vec<T, N> as an explicit template argument of a function template.
#include <metal_stdlib>
using namespace metal;
template <typename T> static T f(T a) { return a; }
kernel void k(device float2* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f<vec<float, 2> >(float2(1)); }
