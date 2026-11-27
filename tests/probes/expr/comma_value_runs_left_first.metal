// EXPECT: valid
// DISASM-ORDER: %float_3 Aligned
// DISASM-ORDER: %float_7 Aligned
// DISASM-MATCH: OpStore %[0-9]+ %float_5
// The left operand of a comma is evaluated for its effect (the store of 3) and
// discarded; the value of the expression is the right operand (7). The second
// comma makes 5 the value of v, which the last store writes.
kernel void comma_value_runs_left_first(device float *o [[buffer(0)]]) {
  o[0] = (o[1] = 3.0f, 7.0f);
  float v = (9.0f, 5.0f);
  o[2] = v;
}
