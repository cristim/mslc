// EXPECT: valid
// Apple accepts "metal::sampler" at program scope with no qualifier ahead of it.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
metal::sampler S(coord::normalized, filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(S, float2(0.25));
}
