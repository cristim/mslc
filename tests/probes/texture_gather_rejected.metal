// EXPECT: error the texture method "gather" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.gather(s, float2(0.25));
}
