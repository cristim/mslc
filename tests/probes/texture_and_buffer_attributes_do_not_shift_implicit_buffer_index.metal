// EXPECT: valid
// An unattributed buffer parameter's implicit index counts buffers only, so a texture or
// sampler parameter before it does not move it.
// REFLECT: { "kind": "Buffer", "metal_index": 0,
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]], device float4* out, uint i [[thread_position_in_grid]])
{ out[i] = t.sample(s, float2(0.25)); }
