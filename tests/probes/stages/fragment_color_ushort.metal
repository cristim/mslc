// EXPECT: valid
// DISASM: OpTypeInt 16 0
// DISASM: OpTypeInt 32 0
// DISASM: OpUConvert
// DISASM-NOT: StorageInputOutput16
struct O { ushort c [[color(0)]]; };
fragment O fragment_color_ushort() {
    O o;
    o.c = ushort(65535);
    return o;
}
