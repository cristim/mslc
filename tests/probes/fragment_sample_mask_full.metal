// EXPECT: valid
// DISASM: BuiltIn SampleMask
struct O { uint m [[sample_mask]]; };
fragment O f() { O o; o.m = 0xffffffffu; return o; }
