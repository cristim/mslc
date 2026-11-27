// EXPECT: error the function attribute "noreturn" is not lowered
#include <metal_stdlib>
using namespace metal;

[[noreturn]] float h(float x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
