// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { char x{false ? 1 : 300}; out[0] = x; }
