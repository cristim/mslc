// EXPECT: valid
// A sampler parameter hides the program-scope one: nothing is embedded.
// REFLECT-NOT: "embedded_sampler"
// REFLECT: "kind": "Sampler"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]], sampler S [[sampler(0)]]) {
  return t.sample(S, float2(0.25));
}
