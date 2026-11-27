// EXPECT: valid
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { .a = in[0], .b = in[1] }; out[0] = s.a + s.b; }
