// EXPECT: error narrowed
kernel void k(device int* out [[buffer(0)]]) { int x { { 1.5f, 2 } }; out[0] = x; }
