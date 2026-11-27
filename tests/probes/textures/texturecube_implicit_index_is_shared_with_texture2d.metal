// EXPECT: valid
// Apple numbers unattributed textures of every dimension from one list, smallest unclaimed first.
// REFLECT: "metal_index": 1, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "c"
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "a"
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> c, texture2d<float> a [[texture(0)]], sampler s) {
  return a.sample(s, float2(0.5)) + c.sample(s, float3(0.5));
}
