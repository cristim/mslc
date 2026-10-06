// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v2float %[0-9]+ %[0-9]+ 3 1
// DISASM-MATCH: = OpFAdd %v2float %[0-9]+ %[0-9]+
// DISASM-MATCH: = OpVectorShuffle %v4float %[0-9]+ %[0-9]+ 0 5 2 4
//
// v.wy += w.xy reads the two lanes out of one load of v, adds, and writes them back through the
// shuffle a plain swizzle store uses.
kernel void compound_swizzle_two_lanes(device float4 *out [[buffer(0)]],
                                       uint i [[thread_position_in_grid]])
{
    float4 x = out[i];
    x.wy += float2(1.0f, 2.0f);
    out[i] = x;
}
