// EXPECT: error case value is not a constant expression
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { int y = in[0]; switch (y) { case (true ? y : 1): break; default: break; } out[0] = 1.0f; }
