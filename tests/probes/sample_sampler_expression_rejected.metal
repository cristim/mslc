// EXPECT: error the first argument of sample has to name a sampler
// Apple accepts sampler(filter::linear) as a temporary.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(sampler(), float2(0.25));
}
