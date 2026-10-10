// EXPECT: valid
// DISASM: OpSConvert
// DISASM: OpBranchConditional
// DISASM-NOT: StorageInputOutput16
struct O { short4 c [[color(0)]]; };
fragment O fragment_color_short_early_returns(const device int* values [[buffer(0)]]) {
    O o;
    o.c = short4(-32768, -1, 123, 32767);
    if (values[0] != 0) return o;
    o.c = short4(32767, 123, -1, -32768);
    return o;
}
