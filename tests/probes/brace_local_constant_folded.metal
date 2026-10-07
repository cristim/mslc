// EXPECT: error supported constant expression
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { 1 + 2 }; out[0] = x; }
