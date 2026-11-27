// EXPECT: valid
// DISASM: = OpConstant %int 2147483647
// DISASM-NOT: = OpConstant %long
//
// The largest value an int holds is still an int: the long rule starts at
// 2147483648.
kernel void literal_decimal_int_max_stays_int(device int* out [[buffer(0)]],
                                              uint i [[thread_position_in_grid]])
{
    out[i] = 2147483647;
}
