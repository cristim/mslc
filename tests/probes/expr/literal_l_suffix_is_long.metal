// EXPECT: valid
// DISASM: = OpConstant %long 7
// DISASM-NOT: = OpConstant %int 7
//
// A small literal with an l suffix is a long, which is not its value's doing: 7l
// used to be a lexer error.
kernel void literal_l_suffix_is_long(device long* out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{
    out[i] = 7l;
}
