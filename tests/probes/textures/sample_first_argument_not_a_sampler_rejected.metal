// EXPECT: error is not a sampler, and sample takes one as its first argument
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(t, float2(0.25));
}
