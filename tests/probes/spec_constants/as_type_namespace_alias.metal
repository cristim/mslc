// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpBitcast %uint
// metal::as_type through a namespace alias of metal.
#include <metal_stdlib>
namespace m = metal;
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = m::as_type<uint>(in[i]);
}
