// EXPECT: valid
// DISASM: = OpConstant %ulong 3
// DISASM-NOT: = OpConstant %uint 3
//
// ull is a ulong.
kernel void literal_ull_suffix_is_ulong(device ulong* out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{
    out[i] = 3ull;
}
