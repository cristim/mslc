// EXPECT: valid
// DISASM-MATCH: = OpUConvert %uint %[0-9]+
// DISASM-MATCH: = OpBitcast %int %[0-9]+
// DISASM-MATCH: = OpShiftLeftLogical %int %[0-9]+ %int_9
// DISASM-MATCH: = OpUConvert %uchar %[0-9]+
//
// uchar c; c <<= 9 shifts the promoted int and truncates, giving 0. A shift of the 8-bit value by 9
// has no defined result.
kernel void compound_narrow_shift_promotes(device uchar *out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    uchar x = out[i];
    x <<= 9;
    out[i] = x;
}
