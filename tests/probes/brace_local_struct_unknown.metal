// EXPECT: error field c is unknown in S initializer
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { .c = 1 }; out[0] = s.a; }
