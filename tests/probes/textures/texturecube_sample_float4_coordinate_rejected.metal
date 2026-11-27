// EXPECT: error the coordinate of sample has to be a float3 or a number
// Apple rejects float4.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float4(0.5));
}
