// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: Location 0
struct O { uint m [[sample_mask]]; float4 c [[color(0)]]; };
fragment O f() { O o; o.m = 5u; o.c = float4(0.75); return o; }
