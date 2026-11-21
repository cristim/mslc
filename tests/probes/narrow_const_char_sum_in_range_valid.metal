// EXPECT: valid
// DISASM: OpIAdd %int %int_100 %int_20
// DISASM: OpSConvert %char
kernel void k(device float* out [[buffer(0)]]) { char y{100 + 20}; out[0] = y; }
