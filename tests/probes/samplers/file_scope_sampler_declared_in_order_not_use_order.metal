// EXPECT: valid
// Binding indices follow declaration order (A then B), whatever order the body uses them in.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "A" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 1, "name": "B" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler A(filter::linear);
constexpr sampler B(filter::nearest);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(B, float2(0.25)) + t.sample(A, float2(0.5));
}
