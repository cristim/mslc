// EXPECT: error pack_float_to_snorm2x16 takes 1 argument, and this call passes 2
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float2* in [[buffer(0)]], device uint* out [[buffer(1)]]) { out[0] = pack_float_to_snorm2x16(in[0], in[1]); }
