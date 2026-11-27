// EXPECT: valid
kernel void k(device uint* out [[buffer(0)]], constant uint* in [[buffer(1)]]) { uint x { { in[0], in[1] } }; out[0] = x; }
