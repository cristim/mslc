// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { uint y{1 - 2}; out[0] = y; }
