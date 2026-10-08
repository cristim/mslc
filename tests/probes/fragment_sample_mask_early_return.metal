// EXPECT: valid
// DISASM: BuiltIn SampleMask
struct O { uint m [[sample_mask]]; };
fragment O f(device uint* masks [[buffer(0)]]) {
    O o; o.m = masks[0];
    if (o.m == 0u) { return o; }
    o.m = 5u; return o;
}
