// EXPECT: valid
// Embedded samplers are per entry point: each one that names S lists it, at its own binding.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(S, float2(0.25));
}
fragment float4 g(texture2d<float> a [[texture(0)]], texture2d<float> b [[texture(1)]]) {
  return b.sample(S, float2(0.25)) + a.sample(S, float2(0.5));
}
