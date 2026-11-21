// EXPECT: valid
// DISASM: OpIAdd %int %int_1 %int_1_0
kernel void k(device float* out [[buffer(0)]]) { const int n = 1 + 1; constexpr int x{n}; out[0] = x; }
