// EXPECT: valid
// DISASM: OpTypeInt 16 1
// DISASM: OpTypeInt 32 1
// DISASM: OpSConvert
// DISASM-NOT: StorageInputOutput16
struct O { short4 c [[color(0)]]; };
fragment O fragment_color_short4() {
    O o;
    o.c = short4(-32768, -1, 123, 32767);
    return o;
}
