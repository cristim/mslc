// EXPECT: valid
// The used sampler is the first embedded one, at the first binding after the texture.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "used" }
// REFLECT-NOT: "name": "unused"
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler unused(filter::linear);
  constexpr sampler used(filter::nearest);
  return t.sample(used, float2(0.25));
}
