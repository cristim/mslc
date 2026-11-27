// EXPECT: error pack_float_to_unorm4x8 takes a float4
#include <metal_stdlib>
using namespace metal;
kernel void k(device const half4* in [[buffer(0)]], device uint* out [[buffer(1)]]) { out[0] = pack_float_to_unorm4x8(in[0]); }
