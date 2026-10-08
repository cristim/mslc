// EXPECT: valid
// DISASM: OpTypeInt 16 0
// DISASM: OpTypeInt 32 0
// DISASM: OpUConvert
// DISASM-NOT: StorageInputOutput16
struct O { ushort3 c [[color(0)]]; };
fragment O fragment_color_ushort3() {
    O o;
    o.c = ushort3(65535, 32768, 17);
    return o;
}
