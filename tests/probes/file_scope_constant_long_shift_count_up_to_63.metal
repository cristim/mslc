// EXPECT: valid
// DISASM: = OpConstant %long 9223372036854775807

//
// The count limit is the left operand's own width, so a long shifts by up to 63.
// (1l << 63) is the smallest long, and 1 less wraps to the largest.
constant long kValue = (1l << 63) - 1;

kernel void file_scope_constant_long_shift_count_up_to_63(device long* out [[buffer(0)]],
                                                          uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
