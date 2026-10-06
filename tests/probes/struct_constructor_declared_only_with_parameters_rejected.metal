// EXPECT: error is declared and never defined
struct S { float x; S(float a); };
kernel void k(device float *out [[buffer(0)]]) { out[0] = 0; }
