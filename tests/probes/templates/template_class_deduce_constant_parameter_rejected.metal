// EXPECT: error is not an integer constant
// Refs #341: slice 5e: N deduced from Vn<T, N> is not lowered (Apple accepts it).
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> struct Vn { T v[N]; };
template <typename T, int N> T sum(Vn<T, N> v) { T s = T(0); for (int i = 0; i < N; ++i) s += v.v[i]; return s; }
kernel void k(device float* out [[buffer(0)]]) { Vn<float, 3> v; v.v[0] = 1.0f; v.v[1] = 2.0f; v.v[2] = 3.0f; out[0] = sum(v); }
