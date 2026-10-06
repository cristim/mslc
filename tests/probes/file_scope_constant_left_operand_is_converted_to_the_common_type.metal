// EXPECT: valid
// DISASM: = OpConstant %long 2147483647

//
// -1 is converted to a uint before the division, so it is 4294967295 / 2.
constant long kValue = -1 / 2u;

kernel void file_scope_constant_left_operand_is_converted_to_the_common_type(device long* out [[buffer(0)]],
                                                                             uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
