// EXPECT: valid
// DIFFCHECK: writes-zero
// DISASM: OpConvertFToS %int
kernel void k(device float* out [[buffer(0)]]) { uint x{(int)-0.5f}; out[0] = x; }
