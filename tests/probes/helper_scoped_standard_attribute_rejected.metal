// EXPECT: error the function attribute "gnu::always_inline" is not lowered
#include <metal_stdlib>
using namespace metal;

[[gnu::always_inline]] float h(float x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
