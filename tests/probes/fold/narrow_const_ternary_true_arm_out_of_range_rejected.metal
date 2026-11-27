// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { char x{true ? 300 : 1}; out[0] = x; }
