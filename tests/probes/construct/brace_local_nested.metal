// EXPECT: valid
struct Inner { float a; float b; }; struct Outer { Inner v; float c; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { Outer s { { in[0], in[1] }, 3 }; out[0] = s.v.a + s.v.b + s.c; }
