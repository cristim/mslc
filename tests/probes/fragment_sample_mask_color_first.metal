// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: Location 0
struct O { float4 c [[color(0)]]; uint m [[sample_mask]]; };
fragment O f() { O o; o.c = float4(0.75); o.m = 10u; return o; }
