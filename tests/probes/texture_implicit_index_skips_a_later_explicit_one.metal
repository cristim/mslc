// EXPECT: valid
// An unattributed texture or sampler takes the smallest index of its kind that no attribute
// in the list claims, wherever the attribute is. Apple's AIR (air.location_index) gives
// a=1, b=0, s0=1, s1=0 here; a counter that ignores the later attributes gives 0 for all.
// REFLECT: "metal_index": 1, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 0, "name": "a"
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "b"
// REFLECT: { "kind": "Sampler", "metal_index": 1, "descriptor": { "set": 1, "binding": 2 }, "param_index": 2, "name": "s0" }
// REFLECT: { "kind": "Sampler", "metal_index": 0, "descriptor": { "set": 1, "binding": 3 }, "param_index": 3, "name": "s1" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a, texture2d<float> b [[texture(0)]], sampler s0, sampler s1 [[sampler(0)]]) {
  return a.read(uint2(0)) + b.sample(s1, float2(0.25)) + a.sample(s0, float2(0.25));
}
