// EXPECT: valid
// DISASM: OpTypeInt 16 1
// DISASM: OpTypeInt 32 1
// DISASM: OpSConvert
// DISASM-NOT: StorageInputOutput16
struct O { short2 c [[color(0)]]; };
fragment O fragment_color_short2() {
    O o;
    o.c = short2(-32768, -1);
    return o;
}
