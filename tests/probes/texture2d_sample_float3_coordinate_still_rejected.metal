// EXPECT: error the coordinate of sample has to be a float2 or a number
// The cube message names float3 and the 2D one still names float2.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(0.5));
}
