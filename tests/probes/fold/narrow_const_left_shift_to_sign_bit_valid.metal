// EXPECT: valid
// DISASM: OpShiftLeftLogical %int %int_2 %int_30
kernel void k(device float* out [[buffer(0)]]) { float x{2 << 30}; out[0] = x; }
