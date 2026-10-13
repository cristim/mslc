// EXPECT: error which is not a texture2d<float, access::read>
#include <metal_stdlib>
using namespace metal;
float4 fetch(texture2d<float, access::read> t) { return t.read(uint2(0)); }
kernel void hit(texture2d<half, access::read> src, device float4* out [[buffer(0)]]) { out[0] = fetch(src); }
