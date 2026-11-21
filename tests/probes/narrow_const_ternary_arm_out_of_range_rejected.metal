// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { char y{false ? 300 : 1 + 299}; out[0] = y; }
