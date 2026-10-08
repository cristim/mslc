// EXPECT: valid
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float3 v { in[0] }; out[0] = v.x + v.y + v.z; }
