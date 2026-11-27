// EXPECT: valid
// DISASM: OpSwitch

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (int(gid)) {
    default: r = 1; break;
    case 5: r = 2; break;
  }
  out[gid] = r;
}
