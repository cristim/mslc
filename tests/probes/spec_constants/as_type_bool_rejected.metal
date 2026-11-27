// EXPECT: error as_type<uint> of a bool value is not lowered
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], device const float* in [[buffer(1)]]) { out[0] = as_type<uint>(in[0] > 1.0f); }
