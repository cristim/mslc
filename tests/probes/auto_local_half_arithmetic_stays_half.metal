// EXPECT: valid
// DISASM-MATCH: OpFAdd %half
kernel void auto_local_half_arithmetic_stays_half(device float *o [[buffer(0)]]) {
  auto h = half(1024) + half(0.25);
  auto f = half(1.5) * 2.0f;
  o[0] = h;
  o[1] = f;
}
