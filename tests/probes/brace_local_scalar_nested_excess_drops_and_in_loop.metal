// EXPECT: valid
// DISASM-MATCH: OpPhi
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { for (int i = 0; i < 4; i++) { float x { { in[0], in[1] > 0 && in[2] > 0 } }; if (in[3] > 0 || in[4] > 0) { out[i] = x; } } }
