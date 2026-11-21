// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { char x{(int)128.5f}; out[0] = x; }
