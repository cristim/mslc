// EXPECT: error is not a shape mslc can name
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> T (f)(T x) { return x; }
kernel void k(device float* o [[buffer(0)]]) { o[0] = 1.0f; }
