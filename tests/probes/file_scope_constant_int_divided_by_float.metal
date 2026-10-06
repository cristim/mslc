// EXPECT: valid
// DISASM: = OpConstant %float 0.5
//
// 1 / 2.0f divides as a float, so it is 0.5 and not the integer quotient 0.
constant float kValue = 1 / 2.0f;

kernel void file_scope_constant_int_divided_by_float(device float* out [[buffer(0)]],
                                                     uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
