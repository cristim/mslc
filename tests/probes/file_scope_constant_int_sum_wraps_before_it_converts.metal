// EXPECT: valid
// DISASM: = OpConstant %float -2.14748365e+09

//
// The int sum wraps to INT_MIN before it is converted to a float.
constant float kValue = (2147483647 + 1) * 1.0f;

kernel void file_scope_constant_int_sum_wraps_before_it_converts(device float* out [[buffer(0)]],
                                                                 uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
