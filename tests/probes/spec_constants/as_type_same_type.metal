// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM-NOT: OpBitcast
// as_type to the operand's own type is the value.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const uint* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = as_type<uint>(in[i]);
}
