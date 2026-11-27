// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v3float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 2 1 0[^ _0-9a-zA-Z]
//
// rgba names the same components as xyzw.
kernel void swizzle_colour_letters(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    out[i] = float4(v.bgr, v.a);
}
