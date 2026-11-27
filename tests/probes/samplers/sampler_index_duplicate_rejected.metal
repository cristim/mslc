// EXPECT: error the sampler index 1 is used by more than one parameter
//
// Apple: "cannot reserve 'sampler' resource location at index 1".
#include <metal_stdlib>
using namespace metal;
kernel void sampler_index_duplicate_rejected(device uint* out [[buffer(0)]], sampler s [[sampler(1)]], sampler r [[sampler(1)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
