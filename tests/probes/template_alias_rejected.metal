// EXPECT: error an alias template is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T> using V2 = vec<T, 2>;
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
