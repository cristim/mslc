// EXPECT: error without its definition is not lowered
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P;
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
