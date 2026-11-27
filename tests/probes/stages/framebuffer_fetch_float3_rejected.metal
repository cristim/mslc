// EXPECT: error reads a colour attachment only as a float4, half4, int4 or uint4

#include <metal_stdlib>
using namespace metal;

fragment float4 framebuffer_fetch_float3_rejected(float3 c [[color(0)]]) { return float4(c, 1.0); }
