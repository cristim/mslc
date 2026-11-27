// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { uchar x{(int)256.5f}; out[0] = x; }
