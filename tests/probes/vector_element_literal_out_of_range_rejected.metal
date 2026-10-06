// EXPECT: error the index 4 is outside a vector of 4 components
kernel void vector_element_literal_out_of_range_rejected(device float *out [[buffer(0)]],
                                                         device const float4 *in [[buffer(1)]])
{
    out[0] = in[0][4];
}
