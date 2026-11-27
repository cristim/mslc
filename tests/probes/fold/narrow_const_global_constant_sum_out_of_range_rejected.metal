// EXPECT: error cannot be narrowed
constant int g = 100 + 30;
kernel void k(device float* out [[buffer(0)]]) { char x{g}; out[0] = x; }
