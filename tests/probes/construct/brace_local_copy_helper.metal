// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float a; float b; }; S make(float a, float b) { S s { a, b }; return s; }
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S t { make(in[0], in[1]) }; out[0] = t.a + t.b; }
