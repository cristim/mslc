// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpBitcast %v4float
// DISASM: OpBitcast %v4half
// DISASM: OpVectorShuffle %v3float
// DISASM: OpVectorShuffle %v3half
// as_type to a three-component vector from a vector of the size of four of its components: the result
// is the first three of the bits seen as four (MSL specification 2.23: as_type<float3>(float4) keeps .xyz).
#include <metal_stdlib>
using namespace metal;
kernel void k(device const uint4* in [[buffer(0)]], device float* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  float3 f = as_type<float3>(in[i]);
  half3 h = as_type<half3>(in[i].xy);
  out[i] = f.x + f.y + f.z + float(h.x + h.y + h.z);
}
