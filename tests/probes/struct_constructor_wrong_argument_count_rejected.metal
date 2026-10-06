// EXPECT: error call to "S::S" passes 2 arguments, and it takes 1
struct S { float x; S(float a) : x(a) {} };
kernel void k(device float *out [[buffer(0)]]) { S s(1.0, 2.0); out[0] = s.x; }
