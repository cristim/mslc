// EXPECT: error numeric_limits of int2 is not lowered
// Apple's compiler gives numeric_limits<int2>::max() as an int2 (metal 32023.883, macOS 26); its lanes could not be
// observed there, so mslc does not lower a vector's limits.
#include <metal_stdlib>
using namespace metal;
kernel void numeric_limits_vector_rejected(device int* out [[buffer(0)]]) { out[0] = numeric_limits<int2>::max().x; }
