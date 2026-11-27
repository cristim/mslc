// EXPECT: valid
// DISASM: OpConstantNull
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float2x2 m {}; out[0] = m[0][0]; }
