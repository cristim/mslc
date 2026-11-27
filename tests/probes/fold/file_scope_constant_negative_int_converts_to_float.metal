// EXPECT: valid
// DISASM: = OpConstant %float -6.5

//
// A negative int is converted as signed.
constant float kValue = -7 + 0.5f;

kernel void file_scope_constant_negative_int_converts_to_float(device float* out [[buffer(0)]],
                                                               uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
