// EXPECT: valid
struct S { float x; float f() const { return this->x; } float g() const { return (*this).x; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 2.0; out[0] = s.f() + s.g(); }
