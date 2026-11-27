// EXPECT: valid
// DISASM-ORDER: %float_1 Aligned
// DISASM-ORDER: %float_2 Aligned
// DISASM-ORDER: %float_2 Aligned
// DISASM-ORDER: %float_4 Aligned
// A comma at the top of an expression statement runs each operand in order.
kernel void comma_statement_sequence(device float *o [[buffer(0)]]) {
  o[0] = 1.0f, o[1] = 2.0f, o[2] = 4.0f;
}
