// EXPECT: error an array of structs field in value struct
struct E { float x; };
struct S { E values[2]; };
kernel void k(device float* out [[buffer(0)]]) { S s {}; out[0] = 0; }
