// EXPECT: error with declared constructors is not lowered yet
struct S { float a; S() = default; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s {}; out[0] = s.a; }
