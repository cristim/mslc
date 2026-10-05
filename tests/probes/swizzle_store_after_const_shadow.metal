// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 5 2 3[^ _0-9a-zA-Z]
//
// A const "a" inside a block does not make the outer, non-const "a" read-only
// once the block has ended.
kernel void swizzle_store_after_const_shadow(device float4 *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    float4 a = float4(0.0);
    {
        const float4 a = float4(1.0);
        out[i] = a;
    }
    a.xy = float2(1.0);
    out[i + 1] = a;
}
