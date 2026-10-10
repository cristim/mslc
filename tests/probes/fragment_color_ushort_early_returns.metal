// EXPECT: valid
// DISASM: OpUConvert
// DISASM: OpBranchConditional
// DISASM-NOT: StorageInputOutput16
struct O { ushort4 c [[color(0)]]; };
fragment O fragment_color_ushort_early_returns(const device int* values [[buffer(0)]]) {
    O o;
    o.c = ushort4(65535, 32768, 17, 0);
    if (values[0] != 0) return o;
    o.c = ushort4(0, 17, 32768, 65535);
    return o;
}
