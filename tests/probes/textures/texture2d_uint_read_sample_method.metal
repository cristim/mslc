// EXPECT: error access::read textures lower only
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src, sampler s) { src.sample(s, float2(0)); }
