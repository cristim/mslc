// EXPECT: valid
// DISASM: OpCompositeConstruct
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float3 v { 1, in[0], 3 }; out[0] = v.x + v.y + v.z; }
