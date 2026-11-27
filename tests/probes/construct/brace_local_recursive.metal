// EXPECT: error recursive value struct
struct S { S child; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { S s {}; out[0] = 0; }
