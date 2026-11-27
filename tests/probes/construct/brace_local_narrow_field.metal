// EXPECT: error requires a supported constant expression
struct S { float a; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { int(in[0]) }; out[0] = s.a; }
