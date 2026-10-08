// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float2 x { { in[0] }, { in[1] } }; out[0] = x.x + x.y; }
