// EXPECT: valid
// DISASM-ORDER: %float_3 Aligned
// DISASM-ORDER: %float_8 Aligned
// Under a cast to void a comma keeps the effects of both operands.
kernel void comma_void_cast(device float *o [[buffer(0)]]) {
  float a = 1.0f, b = 2.0f;
  (void)(o[0] = 3.0f, o[1] = 8.0f);
  (void)(a, b);
}
