// EXPECT: valid
// DISASM: OpSelect %int %false %int_300 %int_1
kernel void k(device float* out [[buffer(0)]]) { char x{false ? 300 : 1}; out[0] = x; }
