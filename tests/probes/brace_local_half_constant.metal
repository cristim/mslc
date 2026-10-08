// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { half x { 0.1f }; out[0] = x; }
