// EXPECT: error nested scalar initialization with excess elements is not lowered yet
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { { in[0], in[1] } }; out[0] = x; }
