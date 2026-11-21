// EXPECT: valid
// DISASM: OpCompositeConstruct %v3int
kernel void k(device float* out [[buffer(0)]]) { constexpr int3 v{int3(1 + 1)}; out[0] = v.x; }
