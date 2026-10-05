// EXPECT: error attribute "sampler" index 16 is out of bounds
//
// Apple: "'sampler' attribute parameter is out of bounds: must be between 0 and 15". The attribute check is in the parser, so a bad index is reported before the parameter is lowered.
#include <metal_stdlib>
using namespace metal;
kernel void sampler_index_16_rejected(device uint* out [[buffer(0)]], sampler s [[sampler(16)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
