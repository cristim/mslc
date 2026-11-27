// EXPECT: error any takes a bool vector, and a numeric vector does not convert to one
// Apple finds no any for an int2.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { out[0] = any(int2(1,0)) ? 1.0f : 0.0f; }
