// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]]) { half y{65504.0f * 2}; out[0] = y; }
