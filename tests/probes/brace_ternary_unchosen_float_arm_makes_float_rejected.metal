// EXPECT: error floating value cannot be narrowed
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { char x{true ? 1 : 2.5f}; (void)x; out[0] = 1.0f; }
