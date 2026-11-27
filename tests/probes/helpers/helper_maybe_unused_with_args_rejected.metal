// EXPECT: error [[maybe_unused]] takes no argument
#include <metal_stdlib>
using namespace metal;

[[maybe_unused(1)]] float h(float x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
