// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { char y{100 + 30}; out[0] = y; }
