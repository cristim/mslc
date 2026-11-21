// EXPECT: error no matching function for call to "sum"; candidates: ; templates: template
// Refs #341: slice 5e: Vn<T, 3> given Vn<float, 4>; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> struct Vn { T v[N]; };
template <typename T> T sum(Vn<T, 3> v) { return v.v[0] + v.v[2]; }
kernel void k(device float* out [[buffer(0)]]) { Vn<float, 4> v; v.v[0] = 1.0f; v.v[2] = 3.0f; out[0] = sum(v); }
