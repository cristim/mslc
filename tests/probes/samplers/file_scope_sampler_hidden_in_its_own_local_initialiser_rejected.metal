// EXPECT: error is not a sampler
// Apple: the local S is in scope in its own initialiser, so sample gets a float4, not a sampler.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  float4 S = t.sample(S, float2(0.25));
  return S;
}
