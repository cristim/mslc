// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { int y = in[0]; char x{false ? 1 : y}; (void)x; out[0] = 1.0f; }
