// EXPECT: valid
// DISASM: = OpConstant %int -3
// DISASM-NOT: = OpConstant %int 2147483644
//
// The folder divides as a signed int: -7 / 2 is -3. It divided the 32 bits as an
// unsigned, which gave 2147483644.
constant int kValue = -7 / 2;

kernel void file_scope_constant_int_division_truncates(device int* out [[buffer(0)]],
                                                       uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
