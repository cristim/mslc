// EXPECT: valid
// DISASM: = OpConstant %float 16777216
//
// 16777217 is not a float, and converting it rounds to 16777216.
constant float kValue = 16777217 + 0.0f;

kernel void file_scope_constant_int_converts_to_float_once(device float* out [[buffer(0)]],
                                                           uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
