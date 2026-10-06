// EXPECT: valid
// DISASM: = OpConstant %int -8
//
// ~7 is -8: the complement of an int is an int.
constant int kValue = ~7;

kernel void file_scope_constant_bitwise_not(device int* out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
