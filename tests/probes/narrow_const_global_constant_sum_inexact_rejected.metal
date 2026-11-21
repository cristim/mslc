// EXPECT: error cannot be narrowed
constant int g = 16777216 + 1;
kernel void k(device float* out [[buffer(0)]]) { float x{g}; out[0] = x; }
