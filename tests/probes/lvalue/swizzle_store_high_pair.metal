// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 0 1 4 5[^ _0-9a-zA-Z]
//
// A store to .zw keeps the old xy.
kernel void swizzle_store_high_pair(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.zw = b[i].xy;
    out[i] = v;
}
