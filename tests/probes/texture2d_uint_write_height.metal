// EXPECT: error write-only textures lower only
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::write> dst, sampler s) { dst.get_height(); }
