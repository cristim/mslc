// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { char x{true ? 1 : 5 % 0}; (void)x; out[0] = 1.0f; }
