// EXPECT: error gather takes one component::x, y, z or w, after the offset
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) { constexpr component c = component::y; return a.gather(s, float2(0.5), c); }
