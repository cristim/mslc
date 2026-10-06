// EXPECT: error call to "S::f" passes 1 arguments, and it takes 0
struct S { float x; float f() const { return x; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1.0; out[0] = s.f(2.0); }
