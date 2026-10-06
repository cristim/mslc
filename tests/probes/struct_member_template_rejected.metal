// EXPECT: error a member template is not lowered
struct S { float x; template <typename T> T g(T a) { return a; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1; out[0] = s.g(1.0f); }
