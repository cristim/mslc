// EXPECT: error requires a supported constant expression
constant int x = 1;
kernel void k(device float* out [[buffer(0)]]) { constexpr int x{(x > 0) ? 1 : 2}; out[0] = x; }
