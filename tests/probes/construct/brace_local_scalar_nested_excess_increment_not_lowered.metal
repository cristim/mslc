// EXPECT: error ++ and -- are lowered only as a statement
kernel void k(device int* out [[buffer(0)]], constant int* in [[buffer(1)]]) { int i = in[0]; int x { { in[1], i++ } }; out[1] = x; out[2] = i; }
