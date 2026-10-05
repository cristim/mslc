// EXPECT: valid
// DISASM-MATCH: OpLoad %v4float %[0-9]+ Aligned 16
//
// A vector element is loaded whole, with the vector's own alignment.
kernel void deref_reads_a_vector(device float4* out [[buffer(0)]],
                                 device const float4* in [[buffer(1)]])
{
    out[0] = *in;
}
