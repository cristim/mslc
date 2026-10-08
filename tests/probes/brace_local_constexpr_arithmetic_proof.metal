// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device int* out [[buffer(0)]]) { constexpr int n {1 + 1}; out[0] = n; }
