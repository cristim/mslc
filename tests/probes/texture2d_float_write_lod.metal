// EXPECT: error mip arguments are unsupported on access::write textures
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::write> dst, uint3 g [[thread_position_in_grid]]) { dst.write(float4(1.0), g.xy, 0); }
