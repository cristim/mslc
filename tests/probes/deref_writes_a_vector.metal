// EXPECT: valid
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 16
//
kernel void deref_writes_a_vector(device float4* out [[buffer(0)]],
                                   device const float4* in [[buffer(1)]])
{
    *out = *in;
}
