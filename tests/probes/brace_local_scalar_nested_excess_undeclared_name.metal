// EXPECT: error "nope" is not a parameter, local, constant or builtin mslc knows about
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], nope } }; out[0] = x; }
