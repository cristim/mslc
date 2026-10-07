// EXPECT: valid
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { in[0], in[1] }; S t { s }; out[0] = t.a + t.b; }
