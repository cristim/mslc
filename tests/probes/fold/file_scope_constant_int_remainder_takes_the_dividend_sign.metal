// EXPECT: valid
// DISASM: = OpConstant %int -1
//
// -7 % 3 is -1, the sign of the dividend. As an unsigned remainder it was 0.
constant int kValue = -7 % 3;

kernel void file_scope_constant_int_remainder_takes_the_dividend_sign(device int* out [[buffer(0)]],
                                                                      uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
