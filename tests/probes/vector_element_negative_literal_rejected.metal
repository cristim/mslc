// EXPECT: error the index -1 is outside a vector of 4 components
kernel void vector_element_negative_literal_rejected(device float *out [[buffer(0)]],
                                                    device const float4 *in [[buffer(1)]])
{
    float4 v = in[0];
    out[0] = v[-1];
}
