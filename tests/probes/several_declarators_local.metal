// EXPECT: valid
// DISASM-MATCH: OpVariable
// A later declarator sees the earlier ones, and each takes its own initialiser.
kernel void several_declarators_local(device float *o [[buffer(0)]]) {
  float a = 1.0, b = a + 1.0, c;
  c = b * 2.0;
  o[0] = a;
  o[1] = b;
  o[2] = c;
}
