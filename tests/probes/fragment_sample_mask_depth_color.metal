// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: BuiltIn FragDepth
// DISASM: DepthReplacing
struct O { uint m [[sample_mask]]; float d [[depth(any)]]; float4 c [[color(0)]]; };
fragment O f() { O o; o.m = 5u; o.d = 0.25; o.c = float4(0.75); return o; }
