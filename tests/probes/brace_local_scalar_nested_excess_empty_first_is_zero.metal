// EXPECT: valid
// DIFFCHECK: writes-zero
// DISASM-MATCH: OpStore %[0-9]+ %float_0[^_0-9]
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { {}, in[1] } }; out[0] = x; }
