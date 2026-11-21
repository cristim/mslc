// EXPECT: valid
// DISASM: OpIAdd %int %int_1 %int_2
kernel void k(device float* out [[buffer(0)]]) { uint y{1 + 2}; out[0] = y; }
