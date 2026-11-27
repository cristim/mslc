// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { int(in[0]) }; out[0] = x; }
