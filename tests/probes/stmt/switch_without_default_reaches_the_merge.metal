// EXPECT: valid
// DISASM: OpSwitch
// A switch with no default label has to send every other value to the block
// after the switch, which is the merge block OpSelectionMerge names.

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 7;
  switch (int(gid)) {
    case 0: r = 1; break;
    case 1: r = 2; break;
  }
  out[gid] = r;
}
