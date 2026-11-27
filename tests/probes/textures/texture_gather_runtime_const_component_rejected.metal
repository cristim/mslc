// EXPECT: error gather requires a constant component
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], constant bool& f [[buffer(0)]]) { const component c = f ? component::y : component::z; return a.gather(s, float2(0.5), int2(0), c); }
