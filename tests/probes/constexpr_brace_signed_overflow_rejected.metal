// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { constexpr int n {2147483647 + 1}; out[0] = n; }
