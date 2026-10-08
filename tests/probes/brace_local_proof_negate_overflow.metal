// EXPECT: error supported constant expression
kernel void k(device float* out [[buffer(0)]]) { const int n = -2147483648; float x { -n }; out[0] = x; }
