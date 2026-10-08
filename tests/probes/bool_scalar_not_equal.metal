// EXPECT: valid
// DISASM: OpLogicalNotEqual
// DISASM-NOT: OpLogicalEqual
kernel void scalar_not_equal(device int *out [[buffer(0)]], constant int2 *in [[buffer(1)]], uint i [[thread_position_in_grid]]) {
    bool a = in[i].x != 0;
    bool b = in[i].y != 0;
    out[i] = int(a != b);
}
