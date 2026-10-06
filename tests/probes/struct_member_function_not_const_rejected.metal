// EXPECT: error is not const
struct S { float x; void set(float a) { x = a; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.set(1.0); out[0] = s.x; }
