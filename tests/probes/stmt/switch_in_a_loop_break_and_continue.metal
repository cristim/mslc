// EXPECT: valid
// DISASM: OpSwitch
// DISASM: OpLoopMerge

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  for (int i = 0; i < 4; ++i) {
    switch (i) {
      case 0: r += 1; break;
      case 1: continue;
      default: r += 100; break;
    }
    r += 1000;
  }
  out[gid] = r;
}
