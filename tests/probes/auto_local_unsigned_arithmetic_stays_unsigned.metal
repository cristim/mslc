// EXPECT: valid
// DISASM-MATCH: OpISub %uint
// DISASM-NOT: OpSDiv
kernel void auto_local_unsigned_arithmetic_stays_unsigned(device float *o [[buffer(0)]]) {
  auto u = 3u;
  auto w = u - 5u;
  o[0] = w > 10u ? 1.0 : 0.0;
}
