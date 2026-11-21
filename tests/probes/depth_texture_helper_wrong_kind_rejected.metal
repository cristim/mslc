// EXPECT: error which is not a depth2d<float>
// Apple: no matching function for call to 'depthAt'.
#include <metal_stdlib>
using namespace metal;
float depthAt(depth2d<float> d, sampler s, float2 p) { return d.sample(s, p); }
fragment float4 hit(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return float4(depthAt(t, s, float2(0.5))); }
