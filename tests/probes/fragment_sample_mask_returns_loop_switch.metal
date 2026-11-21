// EXPECT: valid
// DISASM-ENTRY-STORES-PER-RETURN: f SampleMask
struct O { float4 c [[color(0)]]; uint m [[sample_mask]]; };
fragment O f(device uint* masks [[buffer(0)]]) {
    O o; o.c = float4(1.0); o.m = 1u;
    for (uint i = 0; i < masks[0]; i++) { if (masks[i] == 3u) { o.m = 7u; return o; } }
    switch (masks[1]) { case 1u: return o; case 2u: o.m = 2u; return o; default: break; }
    return o;
}
