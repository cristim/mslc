// EXPECT: valid
// DISASM-MATCH: OpLoad %mat4v4float %[0-9]+ Aligned 4
//
kernel void deref_reads_a_matrix(device float4* out [[buffer(0)]],
                                  device const float4x4* in [[buffer(1)]])
{
    float4x4 m = *in;
    out[0] = m[1];
}
