// EXPECT: valid
// DISASM: OpConvertFToS %int %float_127_900002
kernel void k(device float* out [[buffer(0)]]) { char x{(int)127.9f}; out[0] = x; }
