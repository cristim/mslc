// EXPECT: error with declared constructors is not lowered yet
struct Inner { float a; Inner() = default; }; struct Outer { Inner v; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { Outer s {}; out[0] = s.v.a; }
