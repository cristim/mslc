// EXPECT: valid
// DISASM: = OpConstant %uint 4294967295
// DISASM-NOT: = OpConstant %long
//
// An unsuffixed hex literal goes int, uint, long, ulong, so 0xffffffff is a uint
// where the decimal 4294967295 beside it is a long.
kernel void literal_hex_above_int_max_is_uint(device uint* out [[buffer(0)]],
                                              uint i [[thread_position_in_grid]])
{
    out[i] = 0xffffffff;
}
