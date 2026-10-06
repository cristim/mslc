// EXPECT: valid
// Apple accepts a program-scope sampler with no qualifier at all.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(S, float2(0.25));
}
