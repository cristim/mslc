// EXPECT: error the sample option bias(...) is not lowered yet
// Apple accepts bias(); it is not lowered.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(1.0), bias(1.0));
}
