// EXPECT: error a threadgroup local "s" with an initialiser is not lowered yet
kernel void k(device float* d [[buffer(0)]]) { threadgroup float s = 1.0; d[0] = s; }
