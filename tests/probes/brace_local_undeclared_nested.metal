// EXPECT: error undeclared type
struct S { Missing child; };
kernel void k(device float* out [[buffer(0)]]) { S s; out[0] = 0; }
