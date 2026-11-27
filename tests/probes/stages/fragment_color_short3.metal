// EXPECT: valid
// DISASM: OpTypeInt 16 1
// DISASM: OpTypeInt 32 1
// DISASM: OpSConvert
// DISASM-NOT: StorageInputOutput16
struct O { short3 c [[color(0)]]; };
fragment O fragment_color_short3() {
    O o;
    o.c = short3(-32768, -1, 123);
    return o;
}
