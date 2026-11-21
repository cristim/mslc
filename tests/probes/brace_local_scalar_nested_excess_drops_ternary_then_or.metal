// EXPECT: valid
// DISASM-MATCH: OpPhi
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], (in[1] > 0 || in[2] > 0) ? 1.0f : 0.0f } }; out[0] = x; if (in[3] > 0 || in[4] > 0) out[1] = 1; }
