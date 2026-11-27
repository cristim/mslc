// EXPECT: valid
kernel void k(device int* out [[buffer(0)]]) { constexpr int n {3}; out[0] = n; }
