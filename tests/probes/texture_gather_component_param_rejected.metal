// EXPECT: error gather requires a constant component
#include <metal_stdlib>
using namespace metal;
float4 g(texture2d<float> a, sampler s, component c) { return a.gather(s, float2(0.5), int2(0), c); }
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) { return g(a, s, component::y); }
