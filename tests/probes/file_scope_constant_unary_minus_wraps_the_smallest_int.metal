// EXPECT: valid
// DISASM: = OpConstant %int -2147483648
//
// Negating the smallest int wraps to itself.
constant int kValue = -(-2147483647 - 1);

kernel void file_scope_constant_unary_minus_wraps_the_smallest_int(device int* out [[buffer(0)]],
                                                                   uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
