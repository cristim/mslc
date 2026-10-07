// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 1 %[0-9]+
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  const int label = 1;
  switch (int(gid)) {
    case label: out[gid] = 5; break;
    default: out[gid] = 6; break;
  }
}
