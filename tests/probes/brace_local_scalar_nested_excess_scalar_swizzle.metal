// EXPECT: error swizzles a value that is not a vector
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], in[1].zz } }; out[0] = x; }
