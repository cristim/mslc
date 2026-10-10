// EXPECT: error mip arguments are unsupported
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src, device uint* o [[buffer(0)]]) { o[0] = src.get_width(1u); }
