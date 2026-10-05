// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
//
// float4(v2, v2) is two pieces of two components each, not four values.
kernel void construct_two_vectors(device float4 *out [[buffer(0)]], constant float2 *v [[buffer(1)]], constant float2 *w [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(v[i], w[i]);
}
