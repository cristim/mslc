// EXPECT: valid
struct Inner { float a; }; struct Outer { Inner v; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { Outer s { { in[0] } }; s.v.a = in[1]; out[0] = s.v.a; }
