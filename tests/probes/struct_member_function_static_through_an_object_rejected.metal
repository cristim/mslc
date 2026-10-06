// EXPECT: error is a static member function
struct S { float x; static float f() { return 1.0; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1.0; out[0] = s.f(); }
