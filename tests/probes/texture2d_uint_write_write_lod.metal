// EXPECT: error texture write takes exactly
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::write> dst, sampler s) { dst.write(1u, uint2(0), 0u); }
