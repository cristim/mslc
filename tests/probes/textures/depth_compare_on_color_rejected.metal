// EXPECT: error the texture method "sample_compare" is not lowered yet
// Apple: sample_compare exists on depth textures only.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) { return float4(a.sample_compare(s, float2(0.5), 0.5)); }
