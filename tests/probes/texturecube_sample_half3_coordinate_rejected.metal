// EXPECT: error the coordinate of sample has to be a float3 or a number
// Apple rejects half3 as it does half2 on a texture2d.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, half3(0.5));
}
