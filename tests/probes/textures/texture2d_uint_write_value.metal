// EXPECT: error texture write returns void and has no value
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::write> dst, sampler s) { uint x = dst.write(1u, uint2(0)); }
