// EXPECT: error needs scalar elements
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float2 x { int2(1, 2) }; out[0] = x.x; }
