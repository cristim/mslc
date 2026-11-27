// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM-NOT: RoundEven
// A function of the file with the name of a <metal_pack> function is called as itself.
#include <metal_stdlib>
using namespace metal;
static uint pack_float_to_unorm4x8(float4 v) { return uint(v.x) + 1u; }
kernel void k(device const float4* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = pack_float_to_unorm4x8(in[i]);
}
