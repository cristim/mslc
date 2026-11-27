// EXPECT: error the member "x" is initialised twice
struct S { float x; S(float a) : x(a), x(a) {} };
kernel void k(device float *out [[buffer(0)]]) { S s(1.0); out[0] = s.x; }
