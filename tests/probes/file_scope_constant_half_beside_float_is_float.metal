// EXPECT: valid
// DISASM: = OpConstant %float 0.19997558

//
// A half beside a float is a float, so the sum keeps float precision.
constant float kValue = 0.1h + 0.1f;

kernel void file_scope_constant_half_beside_float_is_float(device float* out [[buffer(0)]],
                                                           uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
