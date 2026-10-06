// EXPECT: valid
// DISASM: = OpConvertFToS %int
//
// `(int)v.x` casts the member, not the vector v.
kernel void cast_applies_to_the_postfix_expression(device int* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0], in[1], in[2]);
    out[i] = (int)v.x;
}
