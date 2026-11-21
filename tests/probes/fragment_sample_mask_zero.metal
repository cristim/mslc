// EXPECT: valid
// DISASM-ENTRY-STORES-PER-RETURN: f SampleMask
// DISASM: BuiltIn SampleMask
struct O { uint m [[sample_mask]]; };
fragment O f() { O o; o.m = 0u; return o; }
