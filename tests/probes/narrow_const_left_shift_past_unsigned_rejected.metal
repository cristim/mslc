// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { float x{4 << 30}; out[0] = x; }
