// EXPECT: error an empty matrix column initializer is not lowered yet
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float2x2 x { {}, {} }; out[0] = x[0][0]; }
