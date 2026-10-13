// EXPECT: error texture write takes exactly a half4 or a number
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<half, access::write> dst, uint3 g [[thread_position_in_grid]]) { dst.write(float4(1.0), g.xy); }
