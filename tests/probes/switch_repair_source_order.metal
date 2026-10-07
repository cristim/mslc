// EXPECT: valid
// DISASM: OpLoad %int %43
// DISASM: OpStore %65 %int_5
// DISASM: OpStore %65 %int_6
// DISASM-NOT: OpStore %43 %int_5
// DISASM-NOT: OpStore %43 %int_6
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int x = 42;
  switch (int(gid)) {
    case 0: out[gid] = x; break;
    case 1: int x; x = 5; out[gid] = x; break;
    default: x = 6; out[gid] = x; break;
  }
  out[gid + 8] = x;
}
