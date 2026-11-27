// EXPECT: valid
// DISASM: OpIAdd %int
// DISASM: OpConvertSToF %float
kernel void k(device float* out [[buffer(0)]]) { const int k = 3; float y{k + 1}; out[0] = y; }
