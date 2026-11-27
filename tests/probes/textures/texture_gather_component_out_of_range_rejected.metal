// EXPECT: error gather takes one component::x, y, z or w (value 0 to 3)
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) { constexpr component c = static_cast<component>(4); return a.gather(s, float2(0.5), int2(0), c); }
