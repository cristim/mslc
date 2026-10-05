// EXPECT: error the third argument of sample has to be level(lod)
// Apple accepts a constant int2 offset; the ConstOffset operand is not lowered.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25), int2(1, 1));
}
