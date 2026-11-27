// EXPECT: error vec<T, N> takes a scalar type
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { vec<float2, 2> v; out[0] = 0; }
