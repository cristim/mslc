// EXPECT: error with declared constructors is not lowered yet
struct S { float a; S(float x) : a(x) {} };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { in[0] }; out[0] = s.a; }
