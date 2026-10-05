// EXPECT: valid
// Apple: sampler index 15 is accepted and 16 is out of bounds.
// REFLECT: { "kind": "Sampler", "metal_index": 15,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(15)]]) {
  return t.sample(s, float2(0.25));
}
