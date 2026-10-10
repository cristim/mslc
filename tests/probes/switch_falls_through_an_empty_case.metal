// EXPECT: valid
// DISASM: OpSwitch
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+( [0-9]+ %[0-9]+)* 0 %[0-9]+
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+( [0-9]+ %[0-9]+)* 1 %[0-9]+

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (int(gid)) {
    case 0:
    case 1: r = 5; break;
    case 2:
      r = 6;
    case 3: r = 7; break;
    default: r = 8;
  }
  out[gid] = r;
}
