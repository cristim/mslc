// EXPECT: valid
struct S { float x; float f() const { return g() * 2.0; } float g() const { return x; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 2.0; out[0] = s.f(); }
