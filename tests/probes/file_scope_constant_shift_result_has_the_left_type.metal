// EXPECT: valid
// DISASM: = OpConstant %long -2147483648

//
// A shift has the type of its promoted left operand alone, so 1 << 31l is an int
// and reaches the sign bit, where the usual arithmetic conversions would make it
// a long and give 2147483648.
constant long kValue = 1 << 31l;

kernel void file_scope_constant_shift_result_has_the_left_type(device long* out [[buffer(0)]],
                                                               uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
