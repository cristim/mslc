// EXPECT: valid
// DISASM: OpSwitch
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+( [0-9]+ %[0-9]+)* 2 %[0-9]+

#include <metal_stdlib>
using namespace metal;

enum Mode { A, B, C };

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  Mode m = Mode(gid % 3);
  int r = 0;
  switch (m) {
    case A: r = 1; break;
    case B: r = 2; break;
    case C: r = 3; break;
  }
  out[gid] = r;
}
