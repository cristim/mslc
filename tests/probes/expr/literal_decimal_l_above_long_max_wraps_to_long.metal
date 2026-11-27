// EXPECT: valid
// DISASM: = OpConstant %long -9223372036854775808
// DISASM-NOT: = OpConstant %ulong
//
// A decimal literal with an l is a long even when it does not fit, as it is
// without the suffix; Apple wraps it.
kernel void literal_decimal_l_above_long_max_wraps_to_long(device long* out [[buffer(0)]],
                                                           uint i [[thread_position_in_grid]])
{
    out[i] = 9223372036854775808l;
}
