// EXPECT: valid
// DISASM-MATCH: OpStore %[0-9]+ %float_1_0 Aligned 4[^%]*OpBranch
// DISASM-ORDER: %float_1_0 Aligned
// DISASM-ORDER: %float_2_0 Aligned
// A conditional whose two arms are casts to void is a statement: one arm runs.
kernel void ternary_void_arms_statement(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float a = 1.0f, b = 2.0f;
  i == 0u ? (void)(o[0] = 1.0f) : (void)(o[1] = 2.0f);
  i == 1u ? (void)(o[2] = a) : (void)(o[3] = b);
}
