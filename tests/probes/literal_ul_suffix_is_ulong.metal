// EXPECT: valid
// DISASM: = OpConstant %ulong 3
// DISASM-NOT: = OpConstant %uint 3
//
// ul is a ulong whatever the value, where 3u is a uint.
kernel void literal_ul_suffix_is_ulong(device ulong* out [[buffer(0)]],
                                       uint i [[thread_position_in_grid]])
{
    out[i] = 3ul;
}
