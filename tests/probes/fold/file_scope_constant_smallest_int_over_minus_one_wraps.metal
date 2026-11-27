// EXPECT: valid
// DISASM: = OpConstant %int -2147483648
//
// The one quotient an int cannot hold wraps to itself, as the divide instruction
// does.
constant int kValue = (-2147483647 - 1) / -1;

kernel void file_scope_constant_smallest_int_over_minus_one_wraps(device int* out [[buffer(0)]],
                                                                  uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
