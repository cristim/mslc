// EXPECT: valid
// DISASM-MATCH: = OpSelect %v2float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
// DISASM-MATCH: = OpConstantComposite %v2float %float_1 %float_1[^_0-9a-zA-Z]
// DISASM-NOT: OpConvertUToF
// DISASM-NOT: OpConvertFToU
//
// float2(bool2) selects 1.0 or 0.0 per component. This is the case in issue 62
// that used to emit OpConvertUToF on a bool vector.
kernel void float_vector_from_bool_vector_is_select(device float2 *out [[buffer(0)]], constant float2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float2(bool2(v[i].x > 0.0, v[i].y > 0.0));
}
