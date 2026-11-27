// EXPECT: valid
// DISASM: OpLogicalEqual
// DISASM: OpLogicalNotEqual
// DISASM: OpSelectionMerge
bool same(bool a, bool b) { return a == b; }
bool different(bool a, bool b) { return a != b; }
kernel void helper_condition(device int *out [[buffer(0)]], constant int2 *in [[buffer(1)]], uint i [[thread_position_in_grid]]) {
    bool a = in[i].x != 0;
    bool b = in[i].y != 0;
    out[i] = 0;
    if (different(a, b) && !same(a, b)) { out[i] = 1; }
}
