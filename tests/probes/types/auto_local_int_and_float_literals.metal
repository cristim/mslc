// EXPECT: valid
// DISASM-MATCH: OpSDiv %int
// DISASM-MATCH: OpFDiv %float
// An int literal deduces int, so 7 / 2 is 3; a floating literal deduces float, which Apple does for 2.5 as well.
kernel void auto_local_int_and_float_literals(device float *o [[buffer(0)]]) {
  auto a = 7;
  auto b = a / 2;
  auto d = 2.5;
  auto e = d / 2;
  o[0] = b + e;
}
