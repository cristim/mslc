// EXPECT: valid
// DISASM: OpCompositeConstruct
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float2x2 m { float2(in[0], 2), float2(3, in[1]) }; out[0] = m[0][0] + m[1][1]; }
