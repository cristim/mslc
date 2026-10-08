// EXPECT: valid
// DISASM: OpIEqual
// DISASM: OpINotEqual
// DISASM-NOT: OpLogicalEqual
// DISASM-NOT: OpLogicalNotEqual
kernel void mixed(device int2 *out [[buffer(0)]], constant int2 *in [[buffer(1)]]) {
    bool a = in[0].y != 0;
    out[0] = int2(int(in[0].x == a), int(in[0].x != a));
}
