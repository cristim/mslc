// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM-NOT: OpBitcast
// A function of the file with a reserved name is called as itself; the builtin as_type is lowered from
// a callee no source can spell.
#include <metal_stdlib>
using namespace metal;
static uint __metal_as_type(float x) { return x > 0.0f ? 7u : 3u; }
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = __metal_as_type(in[i]);
}
