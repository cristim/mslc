// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 5 2 3[^ _0-9a-zA-Z]
//
// A store to .xy takes the value's two lanes and keeps the old zw.
kernel void swizzle_store_pair(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.xy = b[i].zw;
    out[i] = v;
}
