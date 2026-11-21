// EXPECT: valid
// DISASM: OpConvertFToS %int %float_1_5
// DISASM: OpIAdd %int
kernel void k(device float* out [[buffer(0)]]) { float x{(int)1.5f + 1}; out[0] = x; }
