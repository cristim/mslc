// EXPECT: valid
// DISASM: = OpConstant %long 0

//
// -1 is converted to a uint, 4294967295, so the remainder is 0.
constant long kValue = 4294967295u % -1;

kernel void file_scope_constant_right_operand_is_converted_to_the_common_type(device long* out [[buffer(0)]],
                                                                              uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
