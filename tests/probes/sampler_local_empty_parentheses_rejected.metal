// EXPECT: error with empty parentheses declares a function
// Apple parses sampler s() as a function declaration and rejects the use.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s();
  return t.sample(s, float2(0.25));
}
