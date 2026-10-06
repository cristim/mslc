// EXPECT: valid
// DISASM: = OpConstant %float 9.00720033e+15

//
// 2^53 + 2^29 + 1 is just above the midpoint of two floats, so it rounds up.
// Converted through a double it first becomes the midpoint, and then rounds down
// to 2^53.
constant float kValue = 9007199791611905l + 0.0f;

kernel void file_scope_constant_int_converts_to_float_with_one_rounding(device float* out [[buffer(0)]],
                                                                        uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
