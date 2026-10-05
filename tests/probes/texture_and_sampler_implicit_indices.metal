// EXPECT: valid
// A texture or sampler parameter with no attribute takes the next index of its own kind
// that no attribute claimed, as Apple's AIR does: here a is 0, b is explicit 5, c is 1.
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "a"
// REFLECT: "metal_index": 5, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "b"
// REFLECT: "metal_index": 1, "descriptor": { "set": 1, "binding": 2 }, "texture_access": "Sample", "param_index": 2, "name": "c"
// REFLECT: { "kind": "Sampler", "metal_index": 0, "descriptor": { "set": 1, "binding": 3 }, "param_index": 3, "name": "s0" }
// REFLECT: { "kind": "Sampler", "metal_index": 3, "descriptor": { "set": 1, "binding": 4 }, "param_index": 4, "name": "s1" }
// REFLECT: { "kind": "Sampler", "metal_index": 1, "descriptor": { "set": 1, "binding": 5 }, "param_index": 5, "name": "s2" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a, texture2d<float> b [[texture(5)]], texture2d<float> c, sampler s0, sampler s1 [[sampler(3)]], sampler s2) {
  return a.sample(s0, float2(0.25)) + b.sample(s1, float2(0.25)) + c.sample(s2, float2(0.25));
}
