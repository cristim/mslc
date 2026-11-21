// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { const int k = 5; float x{k / 0}; out[0] = x; }
