// EXPECT: valid
// DISASM-MATCH: OpVectorShuffle %v3float %[0-9]+ %[0-9]+ 2 1 0
//
// "(*p).zyx" swizzles the loaded element. The parentheses matter: "*p.zyx"
// would dereference a swizzle of the pointer, which is not a thing.
kernel void deref_swizzle_reads_components(device float3* out [[buffer(0)]],
                                           device const float4* in [[buffer(1)]])
{
    out[0] = (*in).zyx;
}
