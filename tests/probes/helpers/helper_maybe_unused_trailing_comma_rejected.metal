// EXPECT: error expected an attribute name in [[...]]
#include <metal_stdlib>
using namespace metal;

[[maybe_unused,]] float h(float x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
