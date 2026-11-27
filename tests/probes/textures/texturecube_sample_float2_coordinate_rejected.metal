// EXPECT: error the coordinate of sample has to be a float3 or a number
// Apple: no matching member function for call to 'sample'.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.5));
}
