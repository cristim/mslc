// EXPECT: valid
// DISASM-MATCH: OpFunctionCall
float g(float v) { return v * 2.0f; }
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], g(in[1]) } }; out[0] = x + g(in[2]); }
