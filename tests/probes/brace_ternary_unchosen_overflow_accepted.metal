// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { char x{false ? 2147483647 + 1 : 2}; (void)x; out[0] = 1.0f; }
