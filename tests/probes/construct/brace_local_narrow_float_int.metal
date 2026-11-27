// EXPECT: error a floating value cannot be narrowed
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { int x { in[0] }; out[0] = x; }
