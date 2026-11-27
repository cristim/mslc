// EXPECT: error has to be a uint2 or a ushort2
// Apple finds no read(int2) either.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> src, device float4* out [[buffer(0)]]) { out[0] = src.read(int2(0)); }
