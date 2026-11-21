// EXPECT: valid
// DISASM: OpConvertFToS %int %float_255_5
kernel void k(device float* out [[buffer(0)]]) { uchar x{(int)255.5f}; out[0] = x; }
