// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 5 6 3[^ _0-9a-zA-Z]
//
// "color.rgb = 0.5" sets three lanes and keeps alpha.
kernel void swizzle_store_literal_broadcast(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 colour = a[i];
    colour.rgb = 0.5;
    out[i] = colour;
}
