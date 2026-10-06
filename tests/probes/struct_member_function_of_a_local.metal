// EXPECT: valid
struct S { float x; float g() const { return x; } float f() const { S t; t.x = x + 1.0; return t.g(); } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 2.0; out[0] = s.f(); }
