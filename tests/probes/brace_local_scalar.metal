// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float f { in[0] }; out[0] = f; }
