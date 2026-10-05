// EXPECT: valid
// Apple's AIR: a=0 (explicit), b=1, c=2 (explicit), d=3; s0=0 (explicit), s1=2, s2=1 (explicit).
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "a"
// REFLECT: "metal_index": 1, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "b"
// REFLECT: "metal_index": 2, "descriptor": { "set": 1, "binding": 2 }, "texture_access": "Sample", "param_index": 2, "name": "c"
// REFLECT: "metal_index": 3, "descriptor": { "set": 1, "binding": 3 }, "texture_access": "Sample", "param_index": 3, "name": "d"
// REFLECT: { "kind": "Sampler", "metal_index": 0, "descriptor": { "set": 1, "binding": 4 }, "param_index": 4, "name": "s0" }
// REFLECT: { "kind": "Sampler", "metal_index": 2, "descriptor": { "set": 1, "binding": 5 }, "param_index": 5, "name": "s1" }
// REFLECT: { "kind": "Sampler", "metal_index": 1, "descriptor": { "set": 1, "binding": 6 }, "param_index": 6, "name": "s2" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a [[texture(0)]], texture2d<float> b, texture2d<float> c [[texture(2)]], texture2d<float> d, sampler s0 [[sampler(0)]], sampler s1, sampler s2 [[sampler(1)]]) {
  return a.read(uint2(0)) + b.read(uint2(0)) + c.read(uint2(0)) + d.read(uint2(0)) + a.sample(s0, float2(0.25)) + b.sample(s1, float2(0.25)) + c.sample(s2, float2(0.25));
}
