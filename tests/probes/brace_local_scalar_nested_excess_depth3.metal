// EXPECT: valid
// DISASM-NOT: OpFunctionCall
float side(float v) { return v * 2.0f; }
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { { in[0], side(in[1]) } } }; out[1] = x; }
