// EXPECT: error gather takes one component::x, y, z or w, after the offset
// Apple: no matching member function for gather(sampler, float2, component).
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.gather(s, float2(0.5), component::w);
}
