// EXPECT: error vec<T, 5> is not lowered: Metal's vectors have 2, 3 or 4 components
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { vec<float, 5> v; out[0] = v[0]; }
