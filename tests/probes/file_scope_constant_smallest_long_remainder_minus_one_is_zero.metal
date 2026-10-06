// EXPECT: valid
// DISASM: = OpConstant %long 0

//
// LONG_MIN % -1 is 0, and is the same trapping instruction as the quotient.
constant long kValue = (1l << 63) % -1;

kernel void file_scope_constant_smallest_long_remainder_minus_one_is_zero(device long* out [[buffer(0)]],
                                                                          uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
