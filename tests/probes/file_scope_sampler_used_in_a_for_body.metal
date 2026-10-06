// EXPECT: valid
// A use inside a for body counts: the sampler is embedded.
// REFLECT: "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  float4 c = float4(0.0);
  for (int i = 0; i < 2; i++) {
    c += t.sample(S, float2(0.25));
  }
  return c;
}
