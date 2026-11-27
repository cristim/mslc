// EXPECT: valid
// DISASM: = OpConstant %long 7
// DISASM-NOT: = OpConstant %int 7
//
// The suffix is not case sensitive.
kernel void literal_uppercase_l_suffix_is_long(device long* out [[buffer(0)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i] = 7L;
}
