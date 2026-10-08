// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { 16777217 }; out[0] = x; }
