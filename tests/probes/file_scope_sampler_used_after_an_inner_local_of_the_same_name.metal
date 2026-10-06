// EXPECT: valid
// The local S lives only in the inner block; the use after it is the program-scope sampler.
// REFLECT: "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  float4 c = float4(0.0);
  {
    float S = 1.0;
    c = float4(S);
  }
  return c + t.sample(S, float2(0.25));
}
