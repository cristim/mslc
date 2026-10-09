// EXPECT: valid
// DISASM: OpStore %56 %int_5
// DISASM: OpStore %56 %int_6
// DISASM: OpLoad %int %43
// DISASM-NOT: OpStore %43 %int_5
// DISASM-NOT: OpStore %43 %int_6
// DISASM-NOT: OpStore %56 %int_0
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int x = 42;
  switch (int(gid)) {
    case 0: break;
    int x;
    case 1: x = 5; out[gid] = x; break;
    default: x = 6; out[gid] = x; break;
  }
  out[gid + 8] = x;
}
