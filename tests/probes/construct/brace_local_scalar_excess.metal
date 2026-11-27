// EXPECT: error excess elements
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float f { 1, 2 }; out[0] = f; }
