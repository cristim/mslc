// EXPECT: valid
// DISASM-ENTRY-STORES-PER-RETURN: fa SampleMask
// DISASM-ENTRY-STORES-PER-RETURN: fb SampleMask
// DISASM-ENTRY-BUILTINS: fa SampleMask
// DISASM-ENTRY-BUILTINS: fb SampleMask
struct O { uint m [[sample_mask]]; };
fragment O fa(device uint* masks [[buffer(0)]]) {
    O o; o.m = masks[0];
    if (o.m == 0u) { return o; }
    o.m = 5u; return o;
}
fragment O fb() { O o; o.m = 10u; return o; }
