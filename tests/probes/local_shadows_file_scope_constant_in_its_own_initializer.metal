// EXPECT: valid
// A same-named file-scope constant is still shadowed by the local being
// declared. The local is an int, so the initializer is the int literal 1 and
// the module stores and loads %int_1. The file-scope float4 `value` is still
// emitted (it is declared), so the claim cannot be "the float4 is absent"; it
// is that nothing reads it and the local is the int, which these needles pin
// together with the int load.
// DISASM: %45 = OpVariable %_ptr_Function_int Function
// DISASM: OpStore %45 %int_1
// DISASM: OpLoad %int %45
// DISASM-NOT: OpStore %45 %float_7
#include <metal_stdlib>
using namespace metal;
constant float4 value = {7, 7, 7, 7};
kernel void local_shadows_file_scope_constant_in_its_own_initializer(device int *out [[buffer(0)]]) {
  int value = 1;
  out[0] = value;
}