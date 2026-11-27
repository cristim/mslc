// EXPECT: error recursive call
struct S { float x; float f(float n) const { return n < 1.0 ? x : f(n - 1.0); } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1.0; out[0] = s.f(3.0); }
