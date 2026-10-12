// EXPECT: error is "t", which is not a texture3d<float>
#include <metal_stdlib>
using namespace metal;
float4 look(texture3d<float> t, sampler s) { return t.sample(s, float3(0.25)); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return look(t, s); }
