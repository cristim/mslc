// EXPECT: valid
kernel void k(device int* out [[buffer(0)]], constant int* in [[buffer(1)]]) { bool x { { in[0] != 0, in[1] != 0 } }; out[0] = x ? 1 : 0; }
