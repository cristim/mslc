// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { float x{1 << 32}; out[0] = x; }
