// EXPECT: error overloading the constructors of "S" is not lowered yet
struct S { float x; S(float a) : x(a) {} S(float a, float b) : x(a + b) {} };
kernel void k(device float *out [[buffer(0)]]) { S s(1.0); out[0] = s.x; }
