// EXPECT: valid
// RUN: grid=16 local=8 cases=4 elems=256
// DISASM-MATCH: OpAny
// DISASM-MATCH: OpAll
// all and any of <metal_relational> on bool vectors, and on a numeric scalar, which converts to bool.
// On an RTX 3060 threads 0 to 3 write 0, 1010, 1111 and 1011.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint2 p = uint2(i, i * 2u);
  float r = 0.0f;
  if (any(p >= uint2(3u, 4u))) r += 1.0f;
  if (all(p >= uint2(1u, 2u))) r += 10.0f;
  if (metal::any(bool3(i == 2u, false, false))) r += 100.0f;
  if (all(i)) r += 1000.0f;
  out[i] = r;
}
