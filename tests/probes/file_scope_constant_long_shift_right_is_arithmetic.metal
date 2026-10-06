// EXPECT: valid
// DISASM: = OpConstant %long -4

//
// The arithmetic right shift at 64 bits: -8l >> 1 is -4.
constant long kValue = -8l >> 1;

kernel void file_scope_constant_long_shift_right_is_arithmetic(device long* out [[buffer(0)]],
                                                               uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
