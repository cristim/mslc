// EXPECT: valid
// DISASM: OpIMul %int %int_3 %int_2
constant int g = 1 + 2;
kernel void k(device float* out [[buffer(0)]]) { float y{g * 2}; out[0] = y; }
