// EXPECT: error a cube has no offset
// Apple: no matching member function for gather(sampler, float3, int2).
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texturecube<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.gather(s, float3(1), int2(1));
}
