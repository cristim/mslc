// EXPECT: valid
// In a kernel with a buffer: address block 0, texture 1, constexpr sampler 2.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 0, "binding": 2 }, "embedded_sampler": 0, "name": "s" }
#include <metal_stdlib>
using namespace metal;
kernel void k(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]])
{ constexpr sampler s(filter::linear); out[0] = t.sample(s, float2(0.25)); }
