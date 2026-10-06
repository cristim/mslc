// EXPECT: valid
// DISASM: = OpConstant %long 3000000000

//
// The same literal fits a long, so it is not narrowed.
constant long2 kValue = { 3000000000, 1 };

kernel void brace_init_wide_value_into_wide_integer_accepted(device long* out [[buffer(0)]],
                                                             uint i [[thread_position_in_grid]])
{
    out[i] = kValue.x;
}
