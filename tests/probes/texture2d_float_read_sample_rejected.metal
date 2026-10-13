// EXPECT: error access::read textures lower only
// Apple has no sample member on an access::read texture.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> src, sampler s, device float4* out [[buffer(0)]]) { out[0] = src.sample(s, float2(0)); }
