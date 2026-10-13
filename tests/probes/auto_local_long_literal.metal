// EXPECT: valid
// DISASM-MATCH: OpSDiv %long
kernel void auto_local_long_literal(device float *o [[buffer(0)]]) {
  auto big = 3000000000;
  o[0] = big / 1000000000;
}
