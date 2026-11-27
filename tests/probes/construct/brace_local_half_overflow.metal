// EXPECT: error cannot be narrowed
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { half x { 70000.0f }; out[0] = x; }
