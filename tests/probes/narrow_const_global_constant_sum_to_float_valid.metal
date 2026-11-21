// EXPECT: valid
// DISASM: OpConvertSToF %float
constant int g = 1 + 2;
kernel void k(device float* out [[buffer(0)]]) { float x{g}; out[0] = x; }
