// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device int* out [[buffer(0)]]) { const int n = 3; { constexpr int n {n}; out[0] = n; } }
