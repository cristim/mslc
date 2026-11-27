// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpBitcast %v3uint
// as_type between three-component vectors of components of the same size.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  uint3 u = as_type<uint3>(float3(in[i], 1.0f, -2.0f));
  out[i] = u.x + u.y + u.z;
}
