// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { const float n = 0.1f; half x { n }; out[0] = x; }
