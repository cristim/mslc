// EXPECT: error supported constant expression
kernel void k(device float* out [[buffer(0)]]) { float x { -1 << 1 }; out[0] = x; }
