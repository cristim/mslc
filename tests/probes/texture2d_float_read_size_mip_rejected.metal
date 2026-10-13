// EXPECT: error mip arguments are unsupported on access::read textures
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<half, access::read> src, device uint* out [[buffer(0)]]) { out[0] = src.get_width(1u); }
