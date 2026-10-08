// EXPECT: error a pointer field in value struct
struct S { device float* p; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s {}; out[0] = 0; }
