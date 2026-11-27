// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 1 5 3[^ _0-9a-zA-Z]
//
// rgba names the same lanes as xyzw: .rb is lanes 0 and 2.
kernel void swizzle_store_colour_letters(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.rb = b[i].xy;
    out[i] = v;
}
