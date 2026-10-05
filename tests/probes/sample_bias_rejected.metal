// EXPECT: error the sample option bias(...) is not lowered yet
// Apple accepts bias(); the Bias operand is not lowered.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25), bias(1.0));
}
