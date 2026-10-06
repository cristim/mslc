// EXPECT: valid
// DISASM: = OpConstant %ulong 3
// DISASM-NOT: = OpConstant %uint 3
//
// The u may come before or after the l.
kernel void literal_lu_suffix_is_ulong(device ulong* out [[buffer(0)]],
                                       uint i [[thread_position_in_grid]])
{
    out[i] = 3LU;
}
