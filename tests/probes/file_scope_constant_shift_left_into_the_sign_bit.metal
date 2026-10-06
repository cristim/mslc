// EXPECT: valid
// DISASM: = OpConstant %int -2147483648
//
// 1 << 31 is the sign bit, which a signed shift keeps as INT_MIN.
constant int kValue = 1 << 31;

kernel void file_scope_constant_shift_left_into_the_sign_bit(device int* out [[buffer(0)]],
                                                             uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
