// EXPECT: valid
// DISASM-NOT: OpConstant %int 99

#include <metal_stdlib>
using namespace metal;

// C++ never reaches a statement before the first label, so r stays 0 for the selector 5.
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (out[gid]) {
    r = 99;
    case 0: r += 1; break;
    default: r += 2;
  }
  out[gid] = r;
}
