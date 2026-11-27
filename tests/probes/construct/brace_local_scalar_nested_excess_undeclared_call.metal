// EXPECT: error function "nofn" is not a builtin mslc recognises
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], nofn(in[1]) } }; out[0] = x; }
