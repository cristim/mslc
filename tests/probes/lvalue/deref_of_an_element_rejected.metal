// EXPECT: error the operand of unary '*' has to be a pointer parameter
//
// "in[0]" is a float, so Apple rejects the '*'. It is reported rather than
// miscompiled as a second index.
kernel void deref_of_an_element_rejected(device float* out [[buffer(0)]],
                                         device const float* in [[buffer(1)]])
{
    out[0] = *in[0];
}
