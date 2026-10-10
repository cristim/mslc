// EXPECT: valid
// Shadowing a same-named local in an inner brace still shadows it, after the
// self-reference fix installed a binding at the point of declaration. The inner
// block gets its own variable (%41), stores 2 into it and reads it back, and
// the store to `out` after the block reads the outer %38 rather than the inner
// one. Pinning the load to %38 is what makes this a guard: if the inner
// declaration stopped shadowing and reused the outer variable, this needle
// would fail.
//
// DISASM: %38 = OpVariable %_ptr_Function_int Function
// DISASM: %41 = OpVariable %_ptr_Function_int Function
// DISASM: OpStore %38 %int_7
// DISASM: OpStore %41 %int_2
// DISASM: OpLoad %int %41
// DISASM: OpLoad %int %38
// DISASM-NOT: OpStore %41 %int_7
kernel void local_self_initializer_keeps_braced_shadowing(device int *out [[buffer(0)]]) {
  int value = 7;
  {
    int value = 2;
    out[0] = value;
  }
  out[0] = value;
}