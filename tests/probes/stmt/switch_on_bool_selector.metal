// EXPECT: valid
// DISASM: OpSwitch

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  bool b = (gid & 1) != 0;
  int r = 0;
  switch (b) {
    case true: r = 1; break;
    case false: r = 2; break;
  }
  out[gid] = r;
}
