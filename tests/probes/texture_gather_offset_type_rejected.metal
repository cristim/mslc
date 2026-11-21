// EXPECT: error the offset of gather has to be an int2
// Apple: no matching member function for gather(sampler, float2, uint2).
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], constant uint2& q [[buffer(0)]]) {
  return a.gather(s, float2(0.5), q);
}
