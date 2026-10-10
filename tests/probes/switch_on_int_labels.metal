// EXPECT: valid
// DISASM: OpSwitch
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+( [0-9]+ %[0-9]+)* 0 %[0-9]+
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+( [0-9]+ %[0-9]+)* 1 %[0-9]+
// DISASM: OpSelectionMerge

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (int(gid)) {
    case 0: r = 10; break;
    case 1: r = 11; break;
    default: r = 99;
  }
  out[gid] = r;
}
