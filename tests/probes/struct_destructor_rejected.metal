// EXPECT: error a destructor is not supported
struct S { float x; ~S() {} };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1; out[0] = s.x; }
