// EXPECT: error write-only textures lower only
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::write> dst, uint3 g [[thread_position_in_grid]]) { dst.read(g.xy); }
