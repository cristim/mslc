// EXPECT: error "q" in the initialiser list of constructor "S::S" is not a member
struct S { float x; S(float a) : q(a) {} };
kernel void k(device float *out [[buffer(0)]]) { S s(1.0); out[0] = s.x; }
