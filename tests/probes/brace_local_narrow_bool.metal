// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { bool x { 2 }; out[0] = x; }
