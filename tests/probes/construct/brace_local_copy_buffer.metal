// EXPECT: valid
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant S* in [[buffer(1)]]) { S t { in[0] }; out[0] = t.a + t.b; }
