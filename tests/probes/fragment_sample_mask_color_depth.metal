// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: BuiltIn FragDepth
struct O { float4 c [[color(0)]]; float d [[depth(any)]]; uint m [[sample_mask]]; };
fragment O f() { O o; o.c = float4(0.75); o.d = 0.25; o.m = 10u; return o; }
