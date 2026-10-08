// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: OpTypeArray %uint
// DISASM: OpCompositeConstruct
// DISASM-NOT: SampleRateShading
// DISASM-NOT: DepthReplacing
struct O { uint m [[sample_mask]]; };
fragment O f() { O o; o.m = 5u; return o; }
