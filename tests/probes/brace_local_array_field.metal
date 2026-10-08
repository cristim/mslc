// EXPECT: error an array field in value struct
struct S { float[2] values; };
kernel void k(device float* out [[buffer(0)]]) { S s {}; out[0] = 0; }
