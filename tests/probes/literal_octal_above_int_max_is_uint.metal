// EXPECT: valid
// DISASM: = OpConstant %uint 2147483648
// DISASM-NOT: = OpConstant %long
//
// Octal follows the hex rule: 020000000000 is 2^31, which is a uint.
kernel void literal_octal_above_int_max_is_uint(device uint* out [[buffer(0)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = 020000000000;
}
