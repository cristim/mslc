// EXPECT: valid
// DISASM: = OpConstant %int 128

//
// The operand of a unary minus is promoted first, so negating the char -128 is
// the int 128 and not the char -128 it would wrap back to.
constant char kSmallest = -128;
constant int kValue = -kSmallest;

kernel void file_scope_constant_unary_minus_promotes_a_char(device int* out [[buffer(0)]],
                                                            uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
