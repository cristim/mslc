// EXPECT: error mip arguments are unsupported on access::read textures
// Apple accepts read(coord, lod); a Vulkan storage image has one level, so mslc refuses it.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> src, device float4* out [[buffer(0)]]) { out[0] = src.read(uint2(0), 1u); }
