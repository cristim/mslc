// EXPECT: valid
// A parameter hides S in f alone: g still names the program-scope S and embeds it.
// REFLECT: "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]], sampler S [[sampler(0)]]) {
  return t.sample(S, float2(0.25));
}
fragment float4 g(texture2d<float> t [[texture(0)]]) {
  return t.sample(S, float2(0.25));
}
