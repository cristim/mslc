// EXPECT: valid
// DISASM: OpReturnValue
// DISASM: BuiltIn SampleMask
struct O { uint m [[sample_mask]]; };
O make_mask(uint m) { O o; o.m = m; return o; }
fragment O f() { return make_mask(5u); }
