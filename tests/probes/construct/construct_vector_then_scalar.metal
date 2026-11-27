// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v4float %[_0-9a-zA-Z]+ %float_1[^ _0-9a-zA-Z]
//
// float4(v3, 1.0) takes the vector's three components and then the scalar, in
// the order written.
kernel void construct_vector_then_scalar(device float4 *out [[buffer(0)]], constant float3 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(v[i], 1.0);
}
