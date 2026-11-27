// EXPECT: valid
// DISASM: OpSelect %int %true %int_1 %int_300
kernel void k(device float* out [[buffer(0)]]) { char x{true ? 1 : 300}; out[0] = x; }
