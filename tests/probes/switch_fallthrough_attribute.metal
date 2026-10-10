// EXPECT: valid
// DISASM: OpSwitch

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (int(gid)) {
    case 0: r = 1; [[fallthrough]];
    case 1: r = 2; break;
    default: r = 3;
  }
  out[gid] = r;
}
