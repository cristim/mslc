// EXPECT: error a local in the threadgroup address space is not lowered yet
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { threadgroup float3 v { 1 }; out[0] = v.x; }
