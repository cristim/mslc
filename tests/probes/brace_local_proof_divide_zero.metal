// EXPECT: error supported constant expression
kernel void k(device float* out [[buffer(0)]]) { half x { 1.0f / 0.0f }; out[0] = x; }
