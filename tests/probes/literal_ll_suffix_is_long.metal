// EXPECT: valid
// DISASM: = OpConstant %long 7
// DISASM-NOT: = OpConstant %int 7
//
// Apple takes ll, and a long long is 64 bits there as a long is.
kernel void literal_ll_suffix_is_long(device long* out [[buffer(0)]],
                                      uint i [[thread_position_in_grid]])
{
    out[i] = 7ll;
}
