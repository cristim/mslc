// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v3float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 0 1 2[^ _0-9a-zA-Z]
//
// A swizzle applies to a parenthesised expression, as in test/lighting.
kernel void swizzle_of_a_product(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4((a[i] * b[i]).xyz, 1.0);
}
