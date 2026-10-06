// EXPECT: error no constructor of "S" takes 0 arguments
struct S { float x; S(float a) : x(a) {} };
kernel void k(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
