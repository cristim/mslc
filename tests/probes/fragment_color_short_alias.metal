// EXPECT: valid
// DISASM: OpTypeInt 16 1
// DISASM: OpTypeInt 32 1
// DISASM: OpSConvert
// DISASM-NOT: StorageInputOutput16
typedef short3 Color;
struct O { Color c [[color(0)]]; };
fragment O fragment_color_short_alias() {
    O o;
    o.c = Color(-32768, -1, 123);
    return o;
}
