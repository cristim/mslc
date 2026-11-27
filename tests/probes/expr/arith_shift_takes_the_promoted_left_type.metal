// EXPECT: valid
// DISASM-MATCH: = OpShiftLeftLogical %int %[0-9]+ %[0-9]+
// DISASM-NOT: = OpShiftLeftLogical %ulong
//
// A shift has the promoted left operand's type; the count does not take part.
// ushort << ulong is an int shift, where the usual conversions would make it ulong.
kernel void arith_shift_takes_the_promoted_left_type(device int *out [[buffer(0)]],
                                                     device const ushort *a [[buffer(1)]],
                                                     device const ulong *n [[buffer(2)]],
                                                     uint i [[thread_position_in_grid]])
{
    out[i] = a[i] << n[i];
}
