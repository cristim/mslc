// EXPECT: valid
// DISASM-ORDER: %float_3 Aligned
// DISASM-ORDER: OpLoad %float
// The right operand of a comma is evaluated after the left one: it reads what
// the left operand stored.
kernel void comma_right_reads_left(device float *o [[buffer(0)]]) {
  o[0] = (o[1] = 3.0f, o[1]);
}
