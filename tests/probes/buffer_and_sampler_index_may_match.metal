// EXPECT: error texture and sampler parameters are not lowered yet
//
// Slots are per kind: buffer(1) and sampler(1) do not collide, as in Apple, so the answer is the existing sampler refusal and not a duplicate-index error.
#include <metal_stdlib>
using namespace metal;
kernel void buffer_and_sampler_index_may_match(device uint* out [[buffer(1)]], sampler s [[sampler(1)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
