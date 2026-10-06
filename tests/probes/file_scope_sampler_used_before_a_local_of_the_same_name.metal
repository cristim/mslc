// EXPECT: valid
// Apple resolves the use before the local declaration to the program-scope sampler.
// REFLECT: "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  float4 c = t.sample(S, float2(0.25));
  float S = 1.0;
  return c * S;
}
