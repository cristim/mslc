// EXPECT: error all takes 1 argument, and this call passes 2
// all takes one argument.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { out[0] = all(bool3(true), true) ? 1.0f : 0.0f; }
