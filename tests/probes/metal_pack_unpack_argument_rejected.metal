// EXPECT: error unpack_unorm4x8_to_float takes a uint
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float* in [[buffer(0)]], device float4* out [[buffer(1)]]) { out[0] = unpack_unorm4x8_to_float(in[0]); }
