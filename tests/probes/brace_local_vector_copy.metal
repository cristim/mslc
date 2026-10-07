// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float3 a = float3(in[0]); float3 v { a }; out[0] = v.z; }
