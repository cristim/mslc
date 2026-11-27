// EXPECT: valid
// DISASM: = OpConstant %long -9223372036854775808

//
// LONG_MIN / -1 does not fit in a long and traps on the host if it is computed
// there, so it wraps to itself by a check of its own.
constant long kValue = (1l << 63) / -1;

kernel void file_scope_constant_smallest_long_over_minus_one_wraps(device long* out [[buffer(0)]],
                                                                   uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
