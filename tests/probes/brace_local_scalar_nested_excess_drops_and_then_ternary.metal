// EXPECT: valid
// DISASM-MATCH: OpPhi
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], in[1] > 0 && in[2] > 0 } }; float y = in[3] > 0 ? in[4] : in[5]; out[0] = x + y; }
