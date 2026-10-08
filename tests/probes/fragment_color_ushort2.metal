// EXPECT: valid
// DISASM: OpTypeInt 16 0
// DISASM: OpTypeInt 32 0
// DISASM: OpUConvert
// DISASM-NOT: StorageInputOutput16
struct O { ushort2 c [[color(0)]]; };
fragment O fragment_color_ushort2() {
    O o;
    o.c = ushort2(65535, 32768);
    return o;
}
