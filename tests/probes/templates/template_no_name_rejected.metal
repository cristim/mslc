// EXPECT: error a template declaration that is not a function is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T> (T x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
