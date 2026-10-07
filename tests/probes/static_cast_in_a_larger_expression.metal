// EXPECT: valid
// DISASM: = OpConvertFToS %int
//
// The '<' of a static_cast is not operator<, and the '>' not operator>: the
// cast nests inside a comparison and a conditional.
kernel void static_cast_in_a_larger_expression(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float x = in[i];
    out[i] = static_cast<int>(x) > 1 ? x : static_cast<float>(i);
}
