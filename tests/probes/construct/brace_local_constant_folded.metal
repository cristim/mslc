// EXPECT: valid
// DISASM: OpIAdd %int %int_1 %int_2
// DISASM: OpConvertSToF %float
kernel void k(device float* out [[buffer(0)]]) { float x { 1 + 2 }; out[0] = x; }
