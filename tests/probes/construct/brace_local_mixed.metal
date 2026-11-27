// EXPECT: valid
struct S { float a; float b; float c; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { .a = in[0], in[1], .c = 3 }; out[0] = s.a + s.b + s.c; }
