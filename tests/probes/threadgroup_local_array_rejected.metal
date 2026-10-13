// EXPECT: error an array of float[4] is not lowered yet
kernel void k(device float* d [[buffer(0)]]) { threadgroup float s[4]; d[0] = s[0]; }
