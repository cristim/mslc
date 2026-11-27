// EXPECT: valid
// DISASM: OpConstant %float 0
struct S { float a; float b; float c; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { .b = in[0] }; out[0] = s.a + s.b + s.c; }
