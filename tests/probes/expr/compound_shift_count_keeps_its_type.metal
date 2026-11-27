// EXPECT: valid
// DISASM-MATCH: = OpShiftLeftLogical %uint %[0-9]+ %[0-9]+
// DISASM-NO-MATCH: OpBitcast
// DISASM-NO-MATCH: OpUConvert
//
// uint x; x <<= n for a long n takes the type of x and leaves the count alone; it is not
// converted to uint first.
kernel void compound_shift_count_keeps_its_type(device uint *out [[buffer(0)]], device const long *n [[buffer(1)]],
                                                uint i [[thread_position_in_grid]])
{
    uint x = out[i];
    x <<= n[i];
    out[i] = x;
}
