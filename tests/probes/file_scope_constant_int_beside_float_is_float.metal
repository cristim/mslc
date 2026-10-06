// EXPECT: valid
// DISASM: = OpConstant %float 2.5
//
// An int beside a float is converted to a float, as it is when the same
// expression is lowered for a local. It was rejected as operands of two types.
constant float kValue = 1 + 1.5f;

kernel void file_scope_constant_int_beside_float_is_float(device float* out [[buffer(0)]],
                                                          uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
