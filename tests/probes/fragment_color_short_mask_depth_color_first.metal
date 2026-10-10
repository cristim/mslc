// EXPECT: valid
// DISASM: OpSConvert
// DISASM: OpUConvert
// DISASM: BuiltIn SampleMask
// DISASM: BuiltIn FragDepth
// DISASM: DepthReplacing
// DISASM-NOT: StorageInputOutput16
struct D { int4 s; uint4 u; uint mask; };
struct O {
    ushort4 u [[color(0)]];
    float d [[depth(any)]];
    short4 s [[color(1)]];
    uint m [[sample_mask]];
};
fragment O fragment_color_short_mask_depth_color_first(const device D* data [[buffer(0)]]) {
    O o;
    o.u = ushort4(data[0].u);
    o.d = 0.25;
    o.s = short4(data[0].s);
    o.m = data[0].mask;
    return o;
}
