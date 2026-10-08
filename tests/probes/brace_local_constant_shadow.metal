// EXPECT: error requires a supported constant expression
constant int n = 1;
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { int n = int(in[0]); float x { n }; out[0] = x; }
