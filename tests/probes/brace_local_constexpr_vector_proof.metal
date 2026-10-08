// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { constexpr float3 n {float3(1.0f)}; out[0] = n.x; }
