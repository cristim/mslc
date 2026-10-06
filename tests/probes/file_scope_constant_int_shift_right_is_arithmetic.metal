// EXPECT: valid
// DISASM: = OpConstant %int -4
//
// A signed right shift keeps the sign: -8 >> 1 is -4, where a logical shift gave
// 2147483644.
constant int kValue = -8 >> 1;

kernel void file_scope_constant_int_shift_right_is_arithmetic(device int* out [[buffer(0)]],
                                                              uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
