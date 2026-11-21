// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { int y = in[0]; char x{true ? 1 : y}; (void)x; out[0] = 1.0f; }
