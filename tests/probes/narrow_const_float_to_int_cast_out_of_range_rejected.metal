// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { float x{(int)3e10f}; out[0] = x; }
