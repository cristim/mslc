// EXPECT: valid
// Slots are per kind: buffer(1) and sampler(1) do not collide, as in Apple.
// REFLECT: { "kind": "Buffer", "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }
// REFLECT: { "kind": "Sampler", "metal_index": 1, "descriptor": { "set": 0, "binding": 1 }
#include <metal_stdlib>
using namespace metal;
kernel void buffer_and_sampler_index_may_match(device uint* out [[buffer(1)]], sampler s [[sampler(1)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
