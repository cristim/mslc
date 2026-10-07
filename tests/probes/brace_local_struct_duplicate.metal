// EXPECT: error unknown or out of order
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { .a = 1, .a = 2 }; out[0] = s.a; }
