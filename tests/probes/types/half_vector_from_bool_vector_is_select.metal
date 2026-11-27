// EXPECT: valid
// DISASM-MATCH: = OpSelect %v4half %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
// DISASM-MATCH: = OpConstantComposite %v4half %half_0x1p_0 %half_0x1p_0 %half_0x1p_0 %half_0x1p_0
//
// One as a half is 0x3C00, not the float's bits.
kernel void half_vector_from_bool_vector_is_select(device half4 *out [[buffer(0)]], constant float4 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = half4(bool4(v[i].x > 0.0, v[i].y > 0.0, v[i].z > 0.0, v[i].w > 0.0));
}
