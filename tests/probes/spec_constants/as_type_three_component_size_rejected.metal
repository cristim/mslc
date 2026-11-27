// EXPECT: error as_type<float3> needs an operand of the same size: it is 16 bytes, and the operand is 8
// A float3 is the size of four floats.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float3* out [[buffer(0)]], device const uint2* in [[buffer(1)]]) { out[0] = as_type<float3>(in[0]); }
