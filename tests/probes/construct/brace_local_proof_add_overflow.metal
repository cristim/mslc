// EXPECT: error supported constant expression
kernel void k(device float* out [[buffer(0)]]) { float x { 2147483647 + 1 }; out[0] = x; }
