// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 7 6 5 4[^ _0-9a-zA-Z]
//
// The value's lanes land in the order the letters name them.
kernel void swizzle_store_reversed(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.wzyx = b[i];
    out[i] = v;
}
