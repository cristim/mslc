// EXPECT: error deducing the length N of vec<T, N> from an argument is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> static T first(vec<T, N> v) { return v[0]; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = first(float3(1)); }
