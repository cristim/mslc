// EXPECT: valid
// DISASM: OpSConvert
// DISASM: OpUConvert
// DISASM: BuiltIn SampleMask
// DISASM: BuiltIn FragDepth
// DISASM: DepthReplacing
// DISASM: OpFunctionCall
// DISASM-NOT: StorageInputOutput16
struct D { int4 s; uint4 u; uint mask; };
struct O {
    uint m [[sample_mask]];
    short4 s [[color(1)]];
    float d [[depth(any)]];
    ushort4 u [[color(0)]];
};
O helper(int4 s, uint4 u, uint mask) {
    O o;
    o.m = mask;
    o.s = short4(s);
    o.d = 0.25;
    o.u = ushort4(u);
    return o;
}
fragment O fragment_color_short_mask_depth_helper(const device D* data [[buffer(0)]]) {
    return helper(data[0].s, data[0].u, data[0].mask);
}
