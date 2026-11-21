// EXPECT: valid
// DISASM-NOT: OpSDiv
kernel void k(device int* out [[buffer(0)]], constant int* in [[buffer(1)]]) { int x { { in[0], in[1] / in[2] } }; out[1] = x; }
