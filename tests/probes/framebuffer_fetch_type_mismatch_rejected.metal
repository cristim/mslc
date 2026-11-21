// EXPECT: error 'color' attachment 0 has different types on input (half4) and output (float4)

#include <metal_stdlib>
using namespace metal;

fragment float4 framebuffer_fetch_type_mismatch_rejected(half4 c [[color(0)]]) { return float4(c); }
