// EXPECT: error variables in function scope cannot be declared static
// Apple: "variables in function scope cannot be declared static".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  static constexpr sampler s(filter::linear);
  return t.sample(s, float2(0.25));
}
