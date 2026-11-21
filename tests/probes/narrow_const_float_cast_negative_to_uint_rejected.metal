// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { uint x{(int)-1.5f}; out[0] = x; }
