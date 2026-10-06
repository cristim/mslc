// EXPECT: valid
// DISASM: = OpConstant %long 4294967296
//
// 0x100000000 does not fit a uint, so it is a long. Read as 32 bits it is 0.
kernel void literal_hex_above_uint_max_is_long(device long* out [[buffer(0)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i] = 0x100000000;
}
