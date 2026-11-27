// EXPECT: valid
struct Inner { float a; float b; }; struct Outer { float a; Inner v; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { Outer s { in[0] }; out[0] = s.a + s.v.a + s.v.b; }
