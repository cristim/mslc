// EXPECT: error has to name a texture or sampler
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler s) { return t.sample(s, float2(0.25)); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return look(t, float2(0.5)); }
