// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { float y{16777216 + 1}; out[0] = y; }
