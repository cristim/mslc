// EXPECT: valid
// DISASM: OpSwitch
// DISASM: OpReturn

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (int(gid)) {
    case 0: out[gid] = 1; return;
    case 1:
    case 2: break;
    default: out[gid] = 2; break;
  }
  out[gid] = r;
}
