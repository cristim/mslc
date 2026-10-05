// EXPECT: valid
// Embedded samplers and their bindings belong to one entry point: the second function's
// first constexpr sampler is embedded_sampler 0 again, at its own binding.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "a" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "b" }
// REFLECT-NOT: "embedded_sampler": 1,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler a(filter::linear);
  return t.sample(a, float2(0.25));
}
fragment float4 g(texture2d<float> t [[texture(0)]]) {
  constexpr sampler b(filter::nearest);
  return t.sample(b, float2(0.25));
}
