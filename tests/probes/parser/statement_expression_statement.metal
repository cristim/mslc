// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A GNU statement expression that is a statement of its own runs as the block it holds: a continue in it
// continues the loop, and a declaration in it is in a scope of its own (n = 100 does not reach the outer n).
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint n = 0;
  for (uint j = 0; j < 8; ++j) {
    ({ if (j == i) { continue; } n += j; });
    ({ uint n = 100; (void)n; });
  }
  out[i] = n;
}
