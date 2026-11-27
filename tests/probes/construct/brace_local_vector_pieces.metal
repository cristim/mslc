// EXPECT: error needs scalar elements
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float4 v { float2(in[0], in[1]), float2(in[2], in[3]) }; out[0] = v.w; }
