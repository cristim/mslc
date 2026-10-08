// EXPECT: error excess elements
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s { 1, 2, 3 }; out[0] = s.a; }
