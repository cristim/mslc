// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 3 2 1 0[^ _0-9a-zA-Z]
//
// The components come out in the order the letters name them.
kernel void swizzle_reversed(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = (a[i] - b[i]).wzyx;
}
