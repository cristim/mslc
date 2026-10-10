// EXPECT: valid
// The outer x is read after the switch and the inner x carries the case
// values. The ids are pinned to the ones this module declares, because the
// claim is which variable each store and load names, not what number the
// id happens to be.
// DISASM: %42 = OpVariable %_ptr_Function_int Function
// DISASM: %56 = OpVariable %_ptr_Function_int Function
// DISASM: OpStore %56 %int_5
// DISASM: OpStore %56 %int_6
// DISASM: OpLoad %int %42
// DISASM-NOT: OpStore %42 %int_5
// DISASM-NOT: OpStore %42 %int_6
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
