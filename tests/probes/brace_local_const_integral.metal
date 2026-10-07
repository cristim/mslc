// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { const int n = 2; float x { n }; out[0] = x; }
