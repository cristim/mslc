// EXPECT: valid
// DISASM-MATCH: OpPhi
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], (in[1] > 0 && in[2] > 0) ? 1.0f : 0.0f } }; bool b = in[3] > 0 && in[4] > 0; out[0] = x; out[1] = b ? 1.0f : 0.0f; }
