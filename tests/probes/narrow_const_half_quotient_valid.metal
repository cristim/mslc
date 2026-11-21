// EXPECT: valid
// DISASM: OpFDiv %float %float_1
// DISASM: OpFConvert %half
kernel void k(device float* out [[buffer(0)]]) { half y{1.0f / 3}; out[0] = y; }
