// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float2x2 x { { in[0], in[1] }, { in[2], in[3] } }; out[0] = x[1][0]; }
