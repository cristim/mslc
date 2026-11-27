// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { constexpr float2 n {in[0], in[1]}; out[0] = n.x; }
