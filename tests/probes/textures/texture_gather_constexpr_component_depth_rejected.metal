// EXPECT: error gather on a depth texture takes no component
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) { constexpr component c = component::y; return a.gather(s, float2(0.5), int2(0), c); }
