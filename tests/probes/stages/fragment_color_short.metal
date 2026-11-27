// EXPECT: valid
// DISASM: OpTypeInt 16 1
// DISASM: OpTypeInt 32 1
// DISASM: OpSConvert
// DISASM-NOT: StorageInputOutput16
struct O { short c [[color(0)]]; };
fragment O fragment_color_short() {
    O o;
    o.c = short(-32768);
    return o;
}
