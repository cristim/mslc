// EXPECT: valid
// DISASM: OpIAdd %int %int_1 %int_1_0
kernel void k(device float* out [[buffer(0)]]) { constexpr int n {1 + 1}; out[0] = n; }
