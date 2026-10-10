// EXPECT: valid
// DISASM: OpTypeInt 16 0
// DISASM: OpTypeInt 32 0
// DISASM: OpUConvert
// DISASM-NOT: StorageInputOutput16
struct O { ushort4 c [[color(0)]]; };
fragment O fragment_color_ushort4() {
    O o;
    o.c = ushort4(65535, 32768, 17, 0);
    return o;
}
