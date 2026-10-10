// EXPECT: error mip arguments are unsupported
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src) { src.read(uint2(0), 0u); }
