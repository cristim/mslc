// EXPECT: valid
// DISASM: = OpConstant %ulong 18446744073709551615
// DISASM-NOT: = OpConstant %long
//
// A hex literal with an l goes long, then ulong, so it does not wrap.
kernel void literal_hex_l_above_long_max_is_ulong(device ulong* out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = 0xffffffffffffffffl;
}
