// EXPECT: valid
kernel void k(device half* out [[buffer(0)]], constant half* in [[buffer(1)]]) { half x { { in[0], in[1] } }; out[0] = x; }
