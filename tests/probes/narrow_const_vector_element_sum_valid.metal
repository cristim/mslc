// EXPECT: valid
// DISASM: OpIAdd %int %int_1 %int_2
// DISASM: OpCompositeConstruct %v3float %float_1 %float_2
kernel void k(device float* out [[buffer(0)]]) { float3 v{1.0f, 2.0f, 1 + 2}; out[0] = v.z; }
