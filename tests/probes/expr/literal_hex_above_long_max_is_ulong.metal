// EXPECT: valid
// DISASM: = OpConstant %ulong 9223372036854775808
// DISASM-NOT: = OpConstant %long
//
// A hex literal that does not fit a long is a ulong, which a decimal one does not
// get (see literal_decimal_above_long_max_wraps_to_long).
kernel void literal_hex_above_long_max_is_ulong(device ulong* out [[buffer(0)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = 0x8000000000000000;
}
