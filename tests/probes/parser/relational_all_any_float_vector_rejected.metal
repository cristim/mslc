// EXPECT: error all takes a bool vector, and a numeric vector does not convert to one
// Apple finds no all for a float2.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { out[0] = all(float2(1)) ? 1.0f : 0.0f; }
