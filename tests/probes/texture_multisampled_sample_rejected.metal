// EXPECT: error the texture2d_ms<float> method "sample" is not lowered yet
// Apple refuses sample on a multisampled texture.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d_ms<float> a [[texture(0)]], sampler s [[sampler(0)]]) { return a.sample(s, float2(0.5)); }
