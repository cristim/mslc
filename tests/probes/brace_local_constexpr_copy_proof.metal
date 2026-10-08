// EXPECT: error a constexpr brace initializer requires a supported constant expression
struct S { float x; };
kernel void k(device float* out [[buffer(0)]]) { constexpr S s {1.0f}; constexpr S n {s}; out[0] = n.x; }
