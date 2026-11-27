// EXPECT: error requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) { float3 v{1.0f, 2.0f, 2147483647 + 1}; out[0] = v.z; }
