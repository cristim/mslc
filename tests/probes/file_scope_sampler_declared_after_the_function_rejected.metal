// EXPECT: error is not a sampler
// Apple: use of undeclared identifier 'S'. The function cannot see a sampler declared below it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(S, float2(0.25));
}
constexpr sampler S(filter::linear);
