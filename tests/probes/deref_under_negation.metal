// EXPECT: valid
// DISASM-MATCH: OpFNegate %float %[0-9]+
//
kernel void deref_under_negation(device float* out [[buffer(0)]],
                                 device const float* in [[buffer(1)]])
{
    out[0] = -*in;
}
