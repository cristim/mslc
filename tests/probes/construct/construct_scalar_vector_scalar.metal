// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v4float %float_0_5 %[_0-9a-zA-Z]+ %float_1[^ _0-9a-zA-Z]
//
// A vector piece between two scalars stays between them.
kernel void construct_scalar_vector_scalar(device float4 *out [[buffer(0)]], constant float2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(0.5, v[i], 1.0);
}
