// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device int* out [[buffer(0)]], constant int* in [[buffer(1)]]) { constexpr int n {in[0]}; out[0] = n; }
