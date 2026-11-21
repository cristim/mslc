// EXPECT: error a constexpr brace initializer requires a supported constant expression
struct S { float a; int b; };
kernel void k(device float* out [[buffer(0)]]) { constexpr S s {1.0f, 1 << 32}; out[0] = s.a; }
