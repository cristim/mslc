// EXPECT: valid
// DISASM: = OpConstant %long -1
// DISASM-NOT: = OpConstant %ulong
//
// Apple types a long long as signed even for a hex literal, so 0xffffffffffffffffll
// is -1 where the same literal with an l is the largest ulong.
kernel void literal_hex_ll_above_long_max_wraps_to_long(device long* out [[buffer(0)]],
                                                        uint i [[thread_position_in_grid]])
{
    out[i] = 0xffffffffffffffffll;
}
