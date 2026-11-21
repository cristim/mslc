// EXPECT: error mip arguments are unsupported on access::write textures
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::write> dst, sampler s) { dst.get_width(0u); }
