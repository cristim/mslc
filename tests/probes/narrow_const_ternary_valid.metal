// EXPECT: valid
// DISASM: OpSelect %int %true %int_1 %int_2
// DISASM: OpConvertSToF %float
kernel void k(device float* out [[buffer(0)]]) { float x{true ? 1 : 2}; out[0] = x; }
