// EXPECT: valid
// DISASM: OpCompositeConstruct
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { in[0], in[1] }; out[0] = s.a + s.b; }
